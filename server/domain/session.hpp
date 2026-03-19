#ifndef QUIZLYX_SERVER_DOMAIN_SESSION_HPP
#define QUIZLYX_SERVER_DOMAIN_SESSION_HPP

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "types.hpp"

namespace quizlyx::server::domain {

constexpr size_t kMaxPlayersPerSession = 50;

struct Player {
  std::string id;
  std::string name;
  Role role;
  int score;
  bool answered_current_question;
  bool connected = true;
  std::optional<std::chrono::steady_clock::time_point> disconnected_at;
};

struct Session {
  std::string id;
  std::string pin;
  std::string quiz_code;
  std::string host_id;
  SessionState state;
  std::vector<Player> players;
  size_t current_question_index;
  std::chrono::steady_clock::time_point question_deadline;
  bool has_question_deadline = false;
  int auto_advance_delay_ms = 0;
};

bool CanJoin(const Session& s);
bool AddPlayer(Session& s, const Player& p);
bool CanStartGame(const Session& s);
bool StartGame(Session& s);
bool CanSubmitAnswer(const Session& s, const std::string& player_id);
bool RecordAnswer(Session& s, const std::string& player_id);
bool AdvanceToNextQuestion(Session& s, size_t total_questions);
void RemovePlayer(Session& s, const std::string& player_id);
bool DisconnectPlayer(Session& s, const std::string& player_id, std::chrono::steady_clock::time_point now);
bool ReconnectPlayer(Session& s, const std::string& player_id);

} // namespace quizlyx::server::domain

#endif // QUIZLYX_SERVER_DOMAIN_SESSION_HPP
