#ifndef QUIZLYX_SERVER_NETWORK_GAME_CONTROLLER_HPP
#define QUIZLYX_SERVER_NETWORK_GAME_CONTROLLER_HPP

#include <atomic>
#include <optional>
#include <string>

#include "domain/answer.hpp"
#include "domain/quiz.hpp"
#include "interfaces/icommand_handler.hpp"
#include "services/session_manager.hpp"
#include "services/session_timer_service.hpp"

namespace quizlyx::server::network {

class GameController {
public:
  GameController(interfaces::ICommandHandler& handler,
                 services::SessionTimerService& timer_service,
                 services::SessionManager& session_manager);

  std::optional<std::string> CreateQuiz(domain::Quiz quiz);
  std::optional<interfaces::SessionCreated> CreateSession(const std::string& quiz_code,
                                                          const std::string& host_id,
                                                          int auto_advance_delay_ms);
  bool StartGame(const std::string& session_id);
  bool NextQuestion(const std::string& session_id);

  std::optional<std::string> JoinAsPlayer(const std::string& session_id,
                                          const std::string& pin,
                                          const std::string& display_name);
  bool LeaveSession(const std::string& session_id, const std::string& player_id);
  bool SubmitAnswer(const std::string& session_id,
                    const std::string& player_id,
                    const domain::PlayerAnswer& answer);

  bool Disconnect(const std::string& session_id, const std::string& player_id);
  bool Reconnect(const std::string& session_id, const std::string& player_id);

private:
  std::string GeneratePlayerId();

  interfaces::ICommandHandler& handler_;
  services::SessionTimerService& timer_service_;
  services::SessionManager& session_manager_;
  std::atomic<size_t> next_player_id_{0};
};

} // namespace quizlyx::server::network

#endif // QUIZLYX_SERVER_NETWORK_GAME_CONTROLLER_HPP
