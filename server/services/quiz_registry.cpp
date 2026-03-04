#include "quiz_registry.hpp"

#include "domain/quiz.hpp"

namespace quizlyx::server::services {

QuizRegistry::QuizRegistry(interfaces::IQuizStorage& storage) : storage_(storage) {}

std::optional<std::string> QuizRegistry::Create(domain::Quiz quiz) {
  if (!domain::Validate(quiz)) return std::nullopt;
  return storage_.Create(std::move(quiz));
}

std::optional<domain::Quiz> QuizRegistry::Get(const std::string& code) const {
  return storage_.Get(code);
}

}  // namespace quizlyx::server::services
