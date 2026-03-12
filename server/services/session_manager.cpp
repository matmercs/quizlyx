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

SessionManager::SessionManager(QuizRegistry& quiz_registry, interfaces::IBroadcastSink& broadcast_sink) :
    quiz_registry_(quiz_registry), broadcast_sink_(broadcast_sink) {
}

std::optional<SessionManager::SessionInfo> SessionManager::CreateSession(const std::string& quiz_code,
                                                                         const std::string& host_id) {
  auto quiz = quiz_registry_.Get(quiz_code);
  if (!quiz)
    return std::nullopt;

  SessionInfo info;
  domain::Session session;

  {
    std::lock_guard lock(mutex_);

    info.session_id = GenerateSessionId();
    info.pin = GeneratePin();

    session.id = info.session_id;
    session.pin = info.pin;
    session.quiz_code = quiz_code;
    session.host_id = host_id;
    session.state = domain::SessionState::Lobby;
    session.current_question_index = 0;
    session.has_question_deadline = false;

    domain::Player host{.id = host_id, .role = domain::Role::Host, .score = 0, .answered_current_question = false};
    session.players.push_back(std::move(host));

    sessions_by_id_.emplace(session.id, session);
    session_id_by_pin_.emplace(session.pin, session.id);
  }

  ::quizlyx::server::events::PlayerJoined joined_event{.player_id = host_id, .role = domain::Role::Host};
  broadcast_sink_.Broadcast(info.session_id, joined_event);

  return info;
}

std::optional<domain::Session> SessionManager::GetSessionById(const std::string& session_id) const {
  std::lock_guard lock(mutex_);
  auto it = sessions_by_id_.find(session_id);
  if (it == sessions_by_id_.end())
    return std::nullopt;
  return it->second;
}

std::optional<domain::Session> SessionManager::GetSessionByPin(const std::string& pin) const {
  std::lock_guard lock(mutex_);
  auto pin_it = session_id_by_pin_.find(pin);
  if (pin_it == session_id_by_pin_.end())
    return std::nullopt;
  auto it = sessions_by_id_.find(pin_it->second);
  if (it == sessions_by_id_.end())
    return std::nullopt;
  return it->second;
}

bool SessionManager::JoinAsPlayer(const std::string& pin, const std::string& player_id) {
  std::string session_id;
  ::quizlyx::server::events::PlayerJoined joined_event{.player_id = player_id, .role = domain::Role::Player};

  {
    std::lock_guard lock(mutex_);

    auto pin_it = session_id_by_pin_.find(pin);
    if (pin_it == session_id_by_pin_.end())
      return false;
    auto it = sessions_by_id_.find(pin_it->second);
    if (it == sessions_by_id_.end())
      return false;

    domain::Session& session = it->second;
    if (!domain::CanJoin(session))
      return false;

    domain::Player player{
        .id = player_id, .role = domain::Role::Player, .score = 0, .answered_current_question = false};
    if (!domain::AddPlayer(session, player))
      return false;

    session_id = session.id;
  }

  broadcast_sink_.Broadcast(session_id, joined_event);
  return true;
}

bool SessionManager::Leave(const std::string& session_id, const std::string& player_id) {
  const std::string& id = session_id;
  ::quizlyx::server::events::PlayerLeft left_event{player_id};

  {
    std::lock_guard lock(mutex_);
    auto it = sessions_by_id_.find(id);
    if (it == sessions_by_id_.end())
      return false;

    domain::Session& session = it->second;
    domain::RemovePlayer(session, player_id);
  }

  broadcast_sink_.Broadcast(id, left_event);
  return true;
}

bool SessionManager::StartGame(const std::string& session_id) {
  const std::string& id = session_id;
  std::optional<::quizlyx::server::events::GameEvent> event;

  {
    std::lock_guard lock(mutex_);

    auto it = sessions_by_id_.find(id);
    if (it == sessions_by_id_.end())
      return false;

    domain::Session& session = it->second;
    auto quiz = quiz_registry_.Get(session.quiz_code);
    if (!quiz || quiz->questions.empty())
      return false;

    if (!domain::StartGame(session))
      return false;

    const auto& question = quiz->questions.at(session.current_question_index);
    auto now = std::chrono::steady_clock::now();
    session.question_deadline = now + question.time_limit_ms;
    session.has_question_deadline = true;

    ::quizlyx::server::events::QuestionStarted qs;
    qs.question_index = session.current_question_index;
    qs.started_at = now;
    qs.duration_ms = question.time_limit_ms;

    event = qs;
  }

  if (event) {
    broadcast_sink_.Broadcast(id, *event);
  }

  return true;
}

bool SessionManager::NextQuestion(const std::string& session_id) {
  const std::string& id = session_id;
  std::optional<::quizlyx::server::events::GameEvent> event;

  {
    std::lock_guard lock(mutex_);

    auto it = sessions_by_id_.find(id);
    if (it == sessions_by_id_.end())
      return false;

    domain::Session& session = it->second;
    auto quiz = quiz_registry_.Get(session.quiz_code);
    if (!quiz || quiz->questions.empty())
      return false;

    const size_t total_questions = quiz->questions.size();
    if (!domain::AdvanceToNextQuestion(session, total_questions))
      return false;

    if (session.state == domain::SessionState::Finished) {
      ::quizlyx::server::events::GameFinished finished;
      finished.final_leaderboard = BuildLeaderboard(session).entries;
      event = finished;
    } else {
      const auto& question = quiz->questions.at(session.current_question_index);
      auto now = std::chrono::steady_clock::now();
      session.question_deadline = now + question.time_limit_ms;
      session.has_question_deadline = true;

      ::quizlyx::server::events::QuestionStarted qs;
      qs.question_index = session.current_question_index;
      qs.started_at = now;
      qs.duration_ms = question.time_limit_ms;
      event = qs;
    }
  }

  if (event) {
    broadcast_sink_.Broadcast(id, *event);
  }

  return true;
}

bool SessionManager::SubmitAnswer(const std::string& session_id,
                                  const std::string& player_id,
                                  const domain::PlayerAnswer& answer) {
  const std::string& id = session_id;
  std::optional<::quizlyx::server::events::GameEvent> event;

  {
    std::lock_guard lock(mutex_);

    auto it = sessions_by_id_.find(id);
    if (it == sessions_by_id_.end())
      return false;

    domain::Session& session = it->second;
    if (!domain::CanSubmitAnswer(session, player_id))
      return false;

    auto quiz = quiz_registry_.Get(session.quiz_code);
    if (!quiz)
      return false;
    if (session.current_question_index >= quiz->questions.size())
      return false;

    const auto& question = quiz->questions.at(session.current_question_index);
    const int points = CalculatePoints(question, answer);

    if (!domain::RecordAnswer(session, player_id))
      return false;

    auto player_it =
        std::ranges::find_if(session.players, [&player_id](const domain::Player& p) { return p.id == player_id; });
    if (player_it == session.players.end())
      return false;

    player_it->score += points;

    event = BuildLeaderboard(session);
  }

  if (event) {
    broadcast_sink_.Broadcast(id, *event);
  }

  return true;
}

std::string SessionManager::GenerateSessionId() {
  return "S" + std::to_string(next_session_id_++);
}

std::string SessionManager::GeneratePin() const {
  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<int> dist(0, 999999);
  const int value = dist(rng);
  return std::format("{:06d}", value);
}

} // namespace quizlyx::server::services
