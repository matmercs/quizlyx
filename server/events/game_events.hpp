#ifndef QUIZLYX_SERVER_EVENTS_GAME_EVENTS_HPP
#define QUIZLYX_SERVER_EVENTS_GAME_EVENTS_HPP

#include <chrono>
#include <string>
#include <variant>
#include <vector>

#include "domain/types.hpp"

namespace quizlyx::server::events {

struct QuestionStarted {
  size_t question_index;
  std::chrono::milliseconds duration_ms;
};

struct TimerUpdate {
  std::chrono::milliseconds remaining_ms;
};

struct QuestionTimeout {};

struct LeaderboardEntry {
  std::string player_id;
  int score;
};

struct Leaderboard {
  std::vector<LeaderboardEntry> entries;
};

struct PlayerJoined {
  std::string player_id;
  domain::Role role;
};

struct PlayerLeft {
  std::string player_id;
};

struct GameFinished {
  std::vector<LeaderboardEntry> final_leaderboard;
};

using GameEvent = std::variant<QuestionStarted, TimerUpdate, QuestionTimeout, Leaderboard,
                              PlayerJoined, PlayerLeft, GameFinished>;

}  // namespace quizlyx::server::events

#endif  // QUIZLYX_SERVER_EVENTS_GAME_EVENTS_HPP
