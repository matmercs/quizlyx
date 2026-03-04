#ifndef QUIZLYX_SERVER_SERVICES_IN_MEMORY_QUIZ_STORAGE_HPP
#define QUIZLYX_SERVER_SERVICES_IN_MEMORY_QUIZ_STORAGE_HPP

#include <mutex>
#include <string>
#include <unordered_map>

#include "domain/quiz.hpp"
#include "interfaces/iquiz_storage.hpp"

namespace quizlyx::server::services {

class InMemoryQuizStorage : public interfaces::IQuizStorage {
 public:
  std::string Create(domain::Quiz quiz) override;
  std::optional<domain::Quiz> Get(const std::string& code) const override;

 private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, domain::Quiz> quizzes_;
  size_t next_id_ = 0;
};

}  // namespace quizlyx::server::services

#endif  // QUIZLYX_SERVER_SERVICES_IN_MEMORY_QUIZ_STORAGE_HPP
