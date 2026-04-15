#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <iostream>
#include <thread>
#include <vector>

#include "app/command_handler.hpp"
#include "interfaces/ibroadcast_sink.hpp"
#include "interfaces/itime_provider.hpp"
#include "network/game_controller.hpp"
#include "network/ws_broadcast_sink.hpp"
#include "network/ws_connection_manager.hpp"
#include "network/ws_server.hpp"
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

} // namespace quizlyx::server

int main(int argc, char** argv) {
  using namespace quizlyx::server;
  namespace net = boost::asio;

  constexpr int TimerIntervalMs = 200;
  constexpr int ReconnectTimeoutMs = 30000;
  constexpr unsigned short DefaultPort = 8080;

  unsigned short port = DefaultPort;
  if (argc > 1) {
    port = static_cast<unsigned short>(std::stoi(argv[1]));
  }

  net::io_context ioc;

  // Service stack
  services::InMemoryQuizStorage quiz_storage;
  services::QuizRegistry quiz_registry{quiz_storage};
  SteadyTimeProvider time_provider;

  // Network components
  network::WsConnectionManager connection_manager;
  network::WsBroadcastSink broadcast_sink{connection_manager};

  // Business logic
  services::SessionManager session_manager{quiz_registry, broadcast_sink, time_provider};
  services::SessionTimerService timer_service{time_provider, std::chrono::milliseconds{TimerIntervalMs}};
  app::ServerCommandHandler command_handler{quiz_registry, session_manager};

  // Game controller (bridges network and business logic)
  network::GameController game_controller{command_handler, timer_service, session_manager};

  // WebSocket server
  auto endpoint = net::ip::tcp::endpoint{net::ip::make_address("0.0.0.0"), port};
  network::WsServer server{ioc, endpoint, game_controller, connection_manager};
  server.Start();

  // Signal handling for clean shutdown
  net::signal_set signals{ioc, SIGINT, SIGTERM};
  signals.async_wait([&](auto, auto) {
    std::cout << "\nShutting down...\n";
    server.Stop();
    ioc.stop();
  });

  // Recurring timer for game ticks
  auto tick_timer = std::make_shared<net::steady_timer>(ioc);
  std::function<void()> schedule_tick;
  schedule_tick = [&, tick_timer]() {
    tick_timer->expires_after(std::chrono::milliseconds{TimerIntervalMs});
    tick_timer->async_wait([&, tick_timer](boost::system::error_code ec) {
      if (ec)
        return;

      auto timer_events = timer_service.Tick();
      for (const auto& te : timer_events) {
        if (te.timer_type == services::TimerType::QuestionDeadline) {
          if (std::holds_alternative<events::TimerUpdate>(te.event)) {
            broadcast_sink.Broadcast(te.session_id, te.event);
          } else if (std::holds_alternative<events::QuestionTimeout>(te.event)) {
            broadcast_sink.Broadcast(te.session_id, te.event);
            auto round_result = game_controller.CompleteQuestion(te.session_id);
            if (!round_result)
              continue;
            broadcast_sink.Broadcast(te.session_id, *round_result);
            if (const auto* reveal = std::get_if<events::AnswerReveal>(&*round_result); reveal != nullptr) {
              timer_service.SetRevealDeadline(te.session_id, time_provider.Now() + reveal->reveal_duration_ms);
            }
          }
        } else if (te.timer_type == services::TimerType::RevealDelay) {
          auto post_reveal_event = game_controller.FinishReveal(te.session_id);
          if (!post_reveal_event)
            continue;
          broadcast_sink.Broadcast(te.session_id, *post_reveal_event);
          if (const auto* leaderboard = std::get_if<events::Leaderboard>(&*post_reveal_event);
              leaderboard != nullptr && leaderboard->next_round_delay_ms.has_value()) {
            timer_service.SetAutoAdvanceDeadline(te.session_id,
                                                 time_provider.Now() + *leaderboard->next_round_delay_ms);
          }
        } else if (te.timer_type == services::TimerType::AutoAdvance) {
          game_controller.NextQuestion(te.session_id);
        }
      }

      // Cleanup players disconnected longer than timeout
      session_manager.CleanupDisconnectedPlayers(std::chrono::milliseconds{ReconnectTimeoutMs});

      schedule_tick();
    });
  };
  schedule_tick();

  std::cout << "Quizlyx server listening on 0.0.0.0:" << port << "\n";

  // Thread pool
  auto thread_count = std::max(1u, std::thread::hardware_concurrency());
  std::vector<std::thread> threads;
  threads.reserve(thread_count - 1);
  for (unsigned i = 1; i < thread_count; ++i) {
    threads.emplace_back([&ioc]() { ioc.run(); });
  }
  ioc.run();

  for (auto& t : threads) {
    t.join();
  }

  std::cout << "Server stopped.\n";
  return 0;
}
