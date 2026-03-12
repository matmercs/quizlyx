#include "session_timer_service.hpp"

namespace quizlyx::server::services {

SessionTimerService::SessionTimerService(interfaces::ITimeProvider& time_provider,
                                         std::chrono::milliseconds update_interval) :
    time_provider_(time_provider), update_interval_(update_interval) {
}

void SessionTimerService::SetDeadline(const std::string& session_id, std::chrono::steady_clock::time_point deadline) {
  std::lock_guard lock(mutex_);
  deadlines_[session_id] = deadline;
}

void SessionTimerService::ClearDeadline(const std::string& session_id) {
  std::lock_guard lock(mutex_);
  deadlines_.erase(session_id);
  last_updates_.erase(session_id);
}

std::vector<SessionTimerService::TimerEvent> SessionTimerService::Tick() {
  const auto now = time_provider_.Now();

  std::vector<TimerEvent> result;
  std::vector<std::string> expired_sessions;

  {
    std::lock_guard lock(mutex_);

    for (const auto& [session_id, deadline] : deadlines_) {
      const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);

      if (remaining <= std::chrono::milliseconds::zero()) {
        result.push_back(TimerEvent{
            .session_id = session_id,
            .event =
                ::quizlyx::server::events::GameEvent{
                    ::quizlyx::server::events::QuestionTimeout{},
                },
        });
        expired_sessions.push_back(session_id);
        continue;
      }

      auto last_it = last_updates_.find(session_id);
      const bool should_emit_update = last_it == last_updates_.end() || now - last_it->second >= update_interval_;

      if (should_emit_update) {
        result.push_back(TimerEvent{
            .session_id = session_id,
            .event =
                ::quizlyx::server::events::GameEvent{
                    ::quizlyx::server::events::TimerUpdate{remaining},
                },
        });
        last_updates_[session_id] = now;
      }
    }

    for (const auto& id : expired_sessions) {
      deadlines_.erase(id);
      last_updates_.erase(id);
    }
  }

  return result;
}

} // namespace quizlyx::server::services
