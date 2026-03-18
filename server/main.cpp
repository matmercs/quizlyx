#include <iostream>

#include "app/command_handler.hpp"
#include "interfaces/ibroadcast_sink.hpp"
#include "interfaces/itime_provider.hpp"
#include "services/in_memory_quiz_storage.hpp"
#include "services/quiz_registry.hpp"
#include "services/session_manager.hpp"
#include "services/session_timer_service.hpp"

namespace quizlyx::server {

class SteadyTimeProvider : public interfaces::ITimeProvider {
public:
  [[nodiscard]] std::chrono::steady_clock::time_point Now() const override {
    return std::chrono::steady_clock::now();
  }
};

class NoOpBroadcastSink : public interfaces::IBroadcastSink {
public:
  void Broadcast(const std::string& /*session_id*/, const events::GameEvent& /*event*/) override {
  }
};

} // namespace quizlyx::server

int main(int argc, char** argv) {
  (void) argc;
  (void) argv;

  using namespace quizlyx::server;

  constexpr int KTimerUpdateIntervalMs = 200;

  services::InMemoryQuizStorage quiz_storage;
  services::QuizRegistry quiz_registry{quiz_storage};
  NoOpBroadcastSink broadcast_sink;
  SteadyTimeProvider time_provider;
  services::SessionManager session_manager{quiz_registry, broadcast_sink, time_provider};
  services::SessionTimerService timer_service{time_provider, std::chrono::milliseconds{KTimerUpdateIntervalMs}};
  app::ServerCommandHandler commands{quiz_registry, session_manager};

  std::cout << "Server starting...\n";
  std::cout << "Service stack initialized. Ready for networking layer.\n";

  return 0;
}
