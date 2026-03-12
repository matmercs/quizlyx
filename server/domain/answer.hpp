#ifndef QUIZLYX_SERVER_DOMAIN_ANSWER_HPP
#define QUIZLYX_SERVER_DOMAIN_ANSWER_HPP

#include <chrono>
#include <vector>

namespace quizlyx::server::domain {

struct PlayerAnswer {
  std::vector<size_t> selected_indices;
  std::chrono::milliseconds time_since_question_start_ms;
};

} // namespace quizlyx::server::domain

#endif // QUIZLYX_SERVER_DOMAIN_ANSWER_HPP
