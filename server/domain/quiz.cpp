#include "quiz.hpp"

namespace quizlyx::server::domain {

namespace {

bool IsIndexValid(size_t idx, size_t options_count) {
  return idx < options_count;
}

} // namespace

bool Validate(const Question& q) {
  if (q.text.empty() || q.options.empty())
    return false;
  if (q.time_limit_ms.count() <= 0)
    return false;

  const size_t n = q.options.size();
  for (size_t idx : q.correct_indices) {
    if (!IsIndexValid(idx, n))
      return false;
  }

  switch (q.answer_type) {
    case AnswerType::SingleChoice:
      return q.correct_indices.size() == 1;
    case AnswerType::MultipleChoice:
      return !q.correct_indices.empty();
  }
  return false;
}

bool Validate(const Quiz& quiz) {
  if (quiz.title.empty() || quiz.questions.empty())
    return false;
  for (const auto& q : quiz.questions) {
    if (!Validate(q))
      return false;
  }
  return true;
}

} // namespace quizlyx::server::domain
