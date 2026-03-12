#ifndef QUIZLYX_SERVER_DOMAIN_QUIZ_HPP
#define QUIZLYX_SERVER_DOMAIN_QUIZ_HPP

#include <chrono>
#include <string>
#include <vector>

#include "types.hpp"

namespace quizlyx::server::domain {

struct Question {
  std::string text;
  AnswerType answer_type;
  std::vector<std::string> options;
  std::vector<size_t> correct_indices;
  std::chrono::milliseconds time_limit_ms;
};

struct Quiz {
  std::string title;
  std::string description;
  std::vector<Question> questions;
};

bool Validate(const Question& q);
bool Validate(const Quiz& quiz);

} // namespace quizlyx::server::domain

#endif // QUIZLYX_SERVER_DOMAIN_QUIZ_HPP
