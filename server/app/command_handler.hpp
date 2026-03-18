#ifndef QUIZLYX_SERVER_APP_COMMAND_HANDLER_HPP
#define QUIZLYX_SERVER_APP_COMMAND_HANDLER_HPP

#include "interfaces/icommand_handler.hpp"
#include "services/quiz_registry.hpp"
#include "services/session_manager.hpp"

namespace quizlyx::server::app {

class ServerCommandHandler : public interfaces::ICommandHandler {
public:
  ServerCommandHandler(services::QuizRegistry& quiz_registry, services::SessionManager& session_manager);

  std::optional<std::string> CreateQuiz(domain::Quiz quiz) override;

  std::optional<interfaces::SessionCreated> CreateSession(const std::string& quiz_code,
                                                          const std::string& host_id) override;

  bool StartGame(const std::string& session_id) override;
  bool NextQuestion(const std::string& session_id) override;

  std::optional<std::string> JoinAsPlayer(const std::string& session_id,
                                          const std::string& pin,
                                          const std::string& display_name) override;
  bool LeaveSession(const std::string& session_id, const std::string& player_id) override;

  bool SubmitAnswer(const std::string& session_id,
                    const std::string& player_id,
                    const domain::PlayerAnswer& answer) override;

private:
  services::QuizRegistry& quiz_registry_;
  services::SessionManager& session_manager_;
};

} // namespace quizlyx::server::app

#endif // QUIZLYX_SERVER_APP_COMMAND_HANDLER_HPP
