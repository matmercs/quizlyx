#ifndef QUIZLYX_SERVER_SERVICES_SESSION_TIMER_SERVICE_HPP
#define QUIZLYX_SERVER_SERVICES_SESSION_TIMER_SERVICE_HPP

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "events/game_events.hpp"
#include "interfaces/itime_provider.hpp"

namespace quizlyx::server::services {

class SessionTimerService {
 public:
  struct TimerEvent {
    std::string session_id;
    ::quizlyx::server::events::GameEvent event;
  };

  SessionTimerService(interfaces::ITimeProvider& time_provider,
                      std::chrono::milliseconds update_interval);

  void SetDeadline(const std::string& session_id,
                   std::chrono::steady_clock::time_point deadline);
  void ClearDeadline(const std::string& session_id);

  std::vector<TimerEvent> Tick();

 private:
  interfaces::ITimeProvider& time_provider_;
  std::chrono::milliseconds update_interval_;

  std::mutex mutex_;
  std::unordered_map<std::string, std::chrono::steady_clock::time_point> deadlines_;
  std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_updates_;
};

}  // namespace quizlyx::server::services

#endif  // QUIZLYX_SERVER_SERVICES_SESSION_TIMER_SERVICE_HPP

