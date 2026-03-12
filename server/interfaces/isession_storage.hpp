#ifndef QUIZLYX_SERVER_INTERFACES_ISESSION_STORAGE_HPP
#define QUIZLYX_SERVER_INTERFACES_ISESSION_STORAGE_HPP

#include <optional>
#include <string>

#include "domain/session.hpp"

namespace quizlyx::server::interfaces {

class ISessionStorage {
public:
  virtual ~ISessionStorage() = default;

  virtual void Store(domain::Session session) = 0;
  virtual void Update(const domain::Session& session) = 0;
  virtual std::optional<domain::Session> GetById(const std::string& id) const = 0;
  virtual std::optional<std::string> FindIdByPin(const std::string& pin) const = 0;
};

} // namespace quizlyx::server::interfaces

#endif // QUIZLYX_SERVER_INTERFACES_ISESSION_STORAGE_HPP
