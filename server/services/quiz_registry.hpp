#ifndef QUIZLYX_SERVER_SERVICES_QUIZ_REGISTRY_HPP
#define QUIZLYX_SERVER_SERVICES_QUIZ_REGISTRY_HPP

#include <optional>
#include <string>

#include "domain/quiz.hpp"
#include "interfaces/iquiz_storage.hpp"

namespace quizlyx::server::services {

class QuizRegistry {
public:
  explicit QuizRegistry(interfaces::IQuizStorage& storage);

  std::optional<std::string> Create(domain::Quiz quiz);
  std::optional<domain::Quiz> Get(const std::string& code) const;

private:
  interfaces::IQuizStorage& storage_;
};

} // namespace quizlyx::server::services

#endif // QUIZLYX_SERVER_SERVICES_QUIZ_REGISTRY_HPP
