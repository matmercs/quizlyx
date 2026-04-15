#ifndef QUIZLYX_SERVER_EVENTS_GAME_EVENTS_HPP
#define QUIZLYX_SERVER_EVENTS_GAME_EVENTS_HPP

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "domain/types.hpp"

namespace quizlyx::server::events {

struct QuestionStarted {
  size_t question_index;
  size_t total_questions;
  std::chrono::steady_clock::time_point started_at;
  std::chrono::milliseconds duration_ms;
  std::string text;
  domain::AnswerType answer_type;
  std::vector<std::string> options;
};

struct TimerUpdate {
  std::chrono::milliseconds remaining_ms;
};

struct QuestionTimeout {};

struct AnswerReveal {
  std::vector<size_t> correct_indices;
  std::vector<int> option_pick_counts;
  std::chrono::milliseconds reveal_duration_ms;
  std::unordered_map<std::string, std::vector<size_t>> selections_by_player;
};

struct LeaderboardEntry {
  std::string player_id;
  int score;
};

struct Leaderboard {
  std::vector<LeaderboardEntry> entries;
  std::optional<std::chrono::milliseconds> next_round_delay_ms;
};

struct PlayerJoined {
  std::string player_id;
  std::string display_name;
  domain::Role role;
  bool is_competing = true;
};

struct PlayerLeft {
  std::string player_id;
};

struct GameFinished {
  std::vector<LeaderboardEntry> final_leaderboard;
};

using GameEvent = std::variant<QuestionStarted,
                               TimerUpdate,
                               QuestionTimeout,
                               AnswerReveal,
                               Leaderboard,
                               PlayerJoined,
                               PlayerLeft,
                               GameFinished>;

} // namespace quizlyx::server::events

#endif // QUIZLYX_SERVER_EVENTS_GAME_EVENTS_HPP
