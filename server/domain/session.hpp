#ifndef QUIZLYX_SERVER_DOMAIN_SESSION_HPP
#define QUIZLYX_SERVER_DOMAIN_SESSION_HPP

#include <chrono>
#include <string>
#include <vector>

#include "types.hpp"

namespace quizlyx::server::domain {

struct Player {
  std::string id;
  Role role;
  int score;
  bool answered_current_question;
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
};

bool CanJoin(const Session& s);
bool AddPlayer(Session& s, const Player& p);
bool CanStartGame(const Session& s);
bool StartGame(Session& s);
bool CanSubmitAnswer(const Session& s, const std::string& player_id);
bool RecordAnswer(Session& s, const std::string& player_id);
bool AdvanceToNextQuestion(Session& s, size_t total_questions);
void RemovePlayer(Session& s, const std::string& player_id);

}  // namespace quizlyx::server::domain

#endif  // QUIZLYX_SERVER_DOMAIN_SESSION_HPP
