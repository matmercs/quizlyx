#include "scoring.hpp"

#include <algorithm>

namespace quizlyx::server::services {

namespace {

const int kBasePoints = 100;
const int kSpeedBonusMax = 100;

bool IsCorrect(const domain::Question& question, const domain::PlayerAnswer& answer) {
  std::vector<size_t> selected = answer.selected_indices;
  std::vector<size_t> correct = question.correct_indices;
  std::ranges::sort(selected);
  std::ranges::sort(correct);
  return selected == correct;
}

} // namespace

int CalculatePoints(const domain::Question& question, const domain::PlayerAnswer& answer) {
  if (!IsCorrect(question, answer))
    return 0;

  const auto time_limit = question.time_limit_ms.count();
  const auto time_used = answer.time_since_question_start_ms.count();
  if (time_limit <= 0)
    return kBasePoints;

  const auto remaining = time_limit - time_used;
  if (remaining <= 0)
    return kBasePoints;

  const int speed_bonus = static_cast<int>(kSpeedBonusMax * remaining / time_limit);
  return kBasePoints + speed_bonus;
}

} // namespace quizlyx::server::services
