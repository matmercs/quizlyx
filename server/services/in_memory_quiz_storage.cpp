#include "in_memory_quiz_storage.hpp"

#include <mutex>

namespace quizlyx::server::services {

std::string InMemoryQuizStorage::Create(domain::Quiz quiz) {
  std::lock_guard lock(mutex_);
  std::string code = "Q" + std::to_string(next_id_++);
  quizzes_.emplace(code, std::move(quiz));
  return code;
}

std::optional<domain::Quiz> InMemoryQuizStorage::Get(const std::string& code) const {
  std::lock_guard lock(mutex_);
  auto it = quizzes_.find(code);
  if (it == quizzes_.end()) return std::nullopt;
  return it->second;
}

}  // namespace quizlyx::server::services
