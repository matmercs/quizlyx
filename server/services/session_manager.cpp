#include "session_manager.hpp"

#include <algorithm>
#include <format>
#include <random>

namespace quizlyx::server::services {

namespace {

::quizlyx::server::events::Leaderboard BuildLeaderboard(const domain::Session& session) {
  ::quizlyx::server::events::Leaderboard leaderboard;
  leaderboard.entries.reserve(session.players.size());
  for (const auto& player : session.players) {
    leaderboard.entries.push_back(events::LeaderboardEntry{.player_id = player.id, .score = player.score});
  }

  std::ranges::sort(leaderboard.entries,
                    [](const ::quizlyx::server::events::LeaderboardEntry& lhs,
                       const ::quizlyx::server::events::LeaderboardEntry& rhs) {
                      if (lhs.score != rhs.score)
                        return lhs.score > rhs.score;
                      return lhs.player_id < rhs.player_id;
                    });

  return leaderboard;
}

} // namespace

SessionManager::SessionManager(QuizRegistry& quiz_registry,
                               interfaces::IBroadcastSink& broadcast_sink,
                               interfaces::ITimeProvider& time_provider) :
    quiz_registry_(quiz_registry), broadcast_sink_(broadcast_sink), time_provider_(time_provider) {
}

SessionManager::SessionEntry* SessionManager::FindEntry(const std::string& session_id) const {
  auto it = sessions_.find(session_id);
  if (it == sessions_.end())
    return nullptr;
  return it->second.get();
}

std::optional<SessionManager::SessionInfo> SessionManager::CreateSession(const std::string& quiz_code,
                                                                         const std::string& host_id,
                                                                         int auto_advance_delay_ms) {
  auto quiz = quiz_registry_.Get(quiz_code);
  if (!quiz)
    return std::nullopt;

  SessionInfo info;

  {
    std::lock_guard lock(global_mutex_);

    info.session_id = GenerateSessionId();
    info.pin = GeneratePin();

    auto entry = std::make_unique<SessionEntry>();
    entry->session.id = info.session_id;
    entry->session.pin = info.pin;
    entry->session.quiz_code = quiz_code;
    entry->session.host_id = host_id;
    entry->session.state = domain::SessionState::Lobby;
    entry->session.current_question_index = 0;
    entry->session.has_question_deadline = false;
    entry->session.auto_advance_delay_ms = auto_advance_delay_ms;

    domain::Player host{
        .id = host_id, .name = host_id, .role = domain::Role::Host, .score = 0, .answered_current_question = false};
    entry->session.players.push_back(std::move(host));

    sessions_.emplace(info.session_id, std::move(entry));
  }

  ::quizlyx::server::events::PlayerJoined joined_event{.player_id = host_id, .role = domain::Role::Host};
  broadcast_sink_.Broadcast(info.session_id, joined_event);

  return info;
}

std::optional<domain::Session> SessionManager::GetSessionById(const std::string& session_id) const {
  SessionEntry* entry = nullptr;
  {
    std::lock_guard lock(global_mutex_);
    entry = FindEntry(session_id);
    if (entry == nullptr)
      return std::nullopt;
  }
  std::lock_guard session_lock(entry->mutex);
  return entry->session;
}

bool SessionManager::JoinAsPlayer(const std::string& session_id,
                                  const std::string& pin,
                                  const std::string& player_id,
                                  const std::string& display_name) {
  SessionEntry* entry = nullptr;
  {
    std::lock_guard lock(global_mutex_);
    entry = FindEntry(session_id);
    if (entry == nullptr)
      return false;
  }

  {
    std::lock_guard session_lock(entry->mutex);

    if (entry->session.pin != pin)
      return false;
    if (!domain::CanJoin(entry->session))
      return false;

    domain::Player player{.id = player_id,
                          .name = display_name,
                          .role = domain::Role::Player,
                          .score = 0,
                          .answered_current_question = false};
    if (!domain::AddPlayer(entry->session, player))
      return false;
  }

  ::quizlyx::server::events::PlayerJoined joined_event{.player_id = player_id, .role = domain::Role::Player};
  broadcast_sink_.Broadcast(session_id, joined_event);
  return true;
}

bool SessionManager::Leave(const std::string& session_id, const std::string& player_id) {
  SessionEntry* entry = nullptr;
  {
    std::lock_guard lock(global_mutex_);
    entry = FindEntry(session_id);
    if (entry == nullptr)
      return false;
  }

  {
    std::lock_guard session_lock(entry->mutex);
    domain::RemovePlayer(entry->session, player_id);
  }

  ::quizlyx::server::events::PlayerLeft left_event{player_id};
  broadcast_sink_.Broadcast(session_id, left_event);
  return true;
}

bool SessionManager::StartGame(const std::string& session_id) {
  SessionEntry* entry = nullptr;
  {
    std::lock_guard lock(global_mutex_);
    entry = FindEntry(session_id);
    if (entry == nullptr)
      return false;
  }

  std::optional<::quizlyx::server::events::GameEvent> event;
  {
    std::lock_guard session_lock(entry->mutex);

    auto quiz = quiz_registry_.Get(entry->session.quiz_code);
    if (!quiz || quiz->questions.empty())
      return false;

    if (!domain::StartGame(entry->session))
      return false;

    const auto& question = quiz->questions.at(entry->session.current_question_index);
    auto now = time_provider_.Now();
    entry->session.question_deadline = now + question.time_limit_ms;
    entry->session.has_question_deadline = true;

    ::quizlyx::server::events::QuestionStarted qs;
    qs.question_index = entry->session.current_question_index;
    qs.started_at = now;
    qs.duration_ms = question.time_limit_ms;

    event = qs;
  }

  if (event) {
    broadcast_sink_.Broadcast(session_id, *event);
  }

  return true;
}

bool SessionManager::NextQuestion(const std::string& session_id) {
  SessionEntry* entry = nullptr;
  {
    std::lock_guard lock(global_mutex_);
    entry = FindEntry(session_id);
    if (entry == nullptr)
      return false;
  }

  std::optional<::quizlyx::server::events::GameEvent> event;
  {
    std::lock_guard session_lock(entry->mutex);

    auto quiz = quiz_registry_.Get(entry->session.quiz_code);
    if (!quiz || quiz->questions.empty())
      return false;

    const size_t total_questions = quiz->questions.size();
    if (!domain::AdvanceToNextQuestion(entry->session, total_questions))
      return false;

    if (entry->session.state == domain::SessionState::Finished) {
      ::quizlyx::server::events::GameFinished finished;
      finished.final_leaderboard = BuildLeaderboard(entry->session).entries;
      event = finished;
    } else {
      const auto& question = quiz->questions.at(entry->session.current_question_index);
      auto now = time_provider_.Now();
      entry->session.question_deadline = now + question.time_limit_ms;
      entry->session.has_question_deadline = true;

      ::quizlyx::server::events::QuestionStarted qs;
      qs.question_index = entry->session.current_question_index;
      qs.started_at = now;
      qs.duration_ms = question.time_limit_ms;
      event = qs;
    }
  }

  if (event) {
    broadcast_sink_.Broadcast(session_id, *event);
  }

  return true;
}

bool SessionManager::SubmitAnswer(const std::string& session_id,
                                  const std::string& player_id,
                                  const domain::PlayerAnswer& answer) {
  SessionEntry* entry = nullptr;
  {
    std::lock_guard lock(global_mutex_);
    entry = FindEntry(session_id);
    if (entry == nullptr)
      return false;
  }

  std::optional<::quizlyx::server::events::GameEvent> event;
  {
    std::lock_guard session_lock(entry->mutex);

    auto quiz = quiz_registry_.Get(entry->session.quiz_code);
    if (!quiz)
      return false;

    if (!domain::CanSubmitAnswer(entry->session, player_id))
      return false;

    if (entry->session.current_question_index >= quiz->questions.size())
      return false;

    const auto& question = quiz->questions.at(entry->session.current_question_index);
    const int points = CalculatePoints(question, answer);

    if (!domain::RecordAnswer(entry->session, player_id))
      return false;

    auto player_it = std::ranges::find_if(entry->session.players,
                                          [&player_id](const domain::Player& p) { return p.id == player_id; });
    if (player_it == entry->session.players.end())
      return false;

    player_it->score += points;

    event = BuildLeaderboard(entry->session);
  }

  if (event) {
    broadcast_sink_.Broadcast(session_id, *event);
  }

  return true;
}

bool SessionManager::Disconnect(const std::string& session_id, const std::string& player_id) {
  SessionEntry* entry = nullptr;
  {
    std::lock_guard lock(global_mutex_);
    entry = FindEntry(session_id);
    if (entry == nullptr)
      return false;
  }

  std::lock_guard session_lock(entry->mutex);
  return domain::DisconnectPlayer(entry->session, player_id, time_provider_.Now());
}

bool SessionManager::Reconnect(const std::string& session_id, const std::string& player_id) {
  SessionEntry* entry = nullptr;
  {
    std::lock_guard lock(global_mutex_);
    entry = FindEntry(session_id);
    if (entry == nullptr)
      return false;
  }

  std::lock_guard session_lock(entry->mutex);
  return domain::ReconnectPlayer(entry->session, player_id);
}

std::vector<std::pair<std::string, std::string>> SessionManager::CleanupDisconnectedPlayers(
    std::chrono::milliseconds timeout) {
  auto now = time_provider_.Now();
  std::vector<std::pair<std::string, std::string>> removed;

  std::vector<std::pair<std::string, SessionEntry*>> entries;
  {
    std::lock_guard lock(global_mutex_);
    for (auto& [id, entry_ptr] : sessions_) {
      entries.emplace_back(id, entry_ptr.get());
    }
  }

  for (auto& [session_id, entry] : entries) {
    std::vector<std::string> to_remove;
    {
      std::lock_guard session_lock(entry->mutex);
      for (const auto& player : entry->session.players) {
        if (!player.connected && player.disconnected_at && now - *player.disconnected_at >= timeout) {
          to_remove.push_back(player.id);
        }
      }
      for (const auto& pid : to_remove) {
        domain::RemovePlayer(entry->session, pid);
        removed.emplace_back(session_id, pid);
      }
    }
    for (const auto& pid : to_remove) {
      broadcast_sink_.Broadcast(session_id, ::quizlyx::server::events::PlayerLeft{pid});
    }
  }

  return removed;
}

std::string SessionManager::GenerateSessionId() {
  return "S" + std::to_string(next_session_id_++);
}

std::string SessionManager::GeneratePin() const {
  constexpr int KPinMaxValue = 999999;
  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<int> dist(0, KPinMaxValue);
  const int value = dist(rng);
  return std::format("{:06d}", value);
}

} // namespace quizlyx::server::services
