#ifndef QUIZLYX_SERVER_INTERFACES_ITIME_PROVIDER_HPP
#define QUIZLYX_SERVER_INTERFACES_ITIME_PROVIDER_HPP

#include <chrono>

namespace quizlyx::server::interfaces {

class ITimeProvider {
public:
  virtual ~ITimeProvider() = default;

  virtual std::chrono::steady_clock::time_point Now() const = 0;
};

} // namespace quizlyx::server::interfaces

#endif // QUIZLYX_SERVER_INTERFACES_ITIME_PROVIDER_HPP
