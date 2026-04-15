#include "session.hpp"

#include <algorithm>

namespace quizlyx::server::domain {

namespace {

Player* FindPlayer(Session& s, const std::string& player_id) {
  auto it = std::ranges::find_if(s.players, [&player_id](const Player& p) { return p.id == player_id; });
  return it != s.players.end() ? &*it : nullptr;
}

const Player* FindPlayer(const Session& s, const std::string& player_id) {
  auto it = std::ranges::find_if(s.players, [&player_id](const Player& p) { return p.id == player_id; });
  return it != s.players.end() ? &*it : nullptr;
}

} // namespace

bool CanJoin(const Session& s) {
  return s.state == SessionState::Lobby;
}

bool AddPlayer(Session& s, const Player& p) {
  if (s.state != SessionState::Lobby)
    return false;
  if (s.players.size() >= kMaxPlayersPerSession)
    return false;
  if (FindPlayer(s, p.id) != nullptr)
    return false;
  s.players.push_back(p);
  return true;
}

bool CanStartGame(const Session& s) {
  return s.state == SessionState::Lobby &&
         std::ranges::any_of(s.players, [](const Player& player) { return player.is_competing; });
}

bool StartGame(Session& s) {
  if (!CanStartGame(s))
    return false;
  s.state = SessionState::Running;
  s.current_question_index = 0;
  for (auto& p : s.players) {
    p.answered_current_question = false;
    p.current_selected_indices.clear();
  }
  return true;
}

bool CanSubmitAnswer(const Session& s, const std::string& player_id) {
  if (s.state != SessionState::Running)
    return false;
  const Player* p = FindPlayer(s, player_id);
  return p != nullptr && s.has_question_deadline && p->is_competing && !p->answered_current_question;
}

bool RecordAnswer(Session& s, const std::string& player_id, const std::vector<size_t>& selected_indices) {
  Player* p = FindPlayer(s, player_id);
  if (p == nullptr)
    return false;
  p->answered_current_question = true;
  p->current_selected_indices = selected_indices;
  return true;
}

bool AdvanceToNextQuestion(Session& s, size_t total_questions) {
  if (s.state != SessionState::Running)
    return false;
  s.current_question_index++;
  for (auto& p : s.players) {
    p.answered_current_question = false;
    p.current_selected_indices.clear();
  }
  if (s.current_question_index >= total_questions) {
    s.state = SessionState::Finished;
  }
  return true;
}

void RemovePlayer(Session& s, const std::string& player_id) {
  auto [first, last] = std::ranges::remove_if(s.players, [&player_id](const Player& p) { return p.id == player_id; });
  s.players.erase(first, last);
}

bool DisconnectPlayer(Session& s, const std::string& player_id, std::chrono::steady_clock::time_point now) {
  Player* p = FindPlayer(s, player_id);
  if (p == nullptr)
    return false;
  p->connected = false;
  p->disconnected_at = now;
  return true;
}

bool ReconnectPlayer(Session& s, const std::string& player_id) {
  Player* p = FindPlayer(s, player_id);
  if (p == nullptr)
    return false;
  p->connected = true;
  p->disconnected_at = std::nullopt;
  return true;
}

} // namespace quizlyx::server::domain
