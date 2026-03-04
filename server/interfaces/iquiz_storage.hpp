#ifndef QUIZLYX_SERVER_INTERFACES_IQUIZ_STORAGE_HPP
#define QUIZLYX_SERVER_INTERFACES_IQUIZ_STORAGE_HPP

#include <optional>
#include <string>

#include "domain/quiz.hpp"

namespace quizlyx::server::interfaces {

class IQuizStorage {
 public:
  virtual ~IQuizStorage() = default;

  virtual std::string Create(domain::Quiz quiz) = 0;
  virtual std::optional<domain::Quiz> Get(const std::string& code) const = 0;
};

}  // namespace quizlyx::server::interfaces

#endif  // QUIZLYX_SERVER_INTERFACES_IQUIZ_STORAGE_HPP
