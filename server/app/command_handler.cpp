#include "command_handler.hpp"

namespace quizlyx::server::app {

ServerCommandHandler::ServerCommandHandler(services::QuizRegistry& quiz_registry,
                                           services::SessionManager& session_manager) :
    quiz_registry_(quiz_registry), session_manager_(session_manager) {
}

std::optional<std::string> ServerCommandHandler::CreateQuiz(domain::Quiz quiz) {
  return quiz_registry_.Create(std::move(quiz));
}

std::optional<interfaces::SessionCreated> ServerCommandHandler::CreateSession(const std::string& quiz_code,
                                                                              const std::string& host_player_id,
                                                                              const std::string& host_display_name,
                                                                              bool host_is_spectator,
                                                                              int auto_advance_delay_ms) {
  auto info = session_manager_.CreateSession(
      quiz_code, host_player_id, host_display_name, host_is_spectator, auto_advance_delay_ms);
  if (!info)
    return std::nullopt;
  return interfaces::SessionCreated{
      .session_id = info->session_id,
      .pin = info->pin,
      .player_id = info->player_id,
      .display_name = info->display_name,
      .is_competing = info->is_competing,
  };
}

bool ServerCommandHandler::StartGame(const std::string& session_id) {
  return session_manager_.StartGame(session_id);
}

bool ServerCommandHandler::NextQuestion(const std::string& session_id) {
  return session_manager_.NextQuestion(session_id);
}

std::optional<interfaces::JoinedSession> ServerCommandHandler::JoinAsPlayer(const std::string& pin,
                                                                            const std::string& player_id,
                                                                            const std::string& display_name) {
  auto info = session_manager_.JoinByPin(pin, player_id, display_name);
  if (!info)
    return std::nullopt;
  return interfaces::JoinedSession{
      .session_id = info->session_id,
      .player_id = info->player_id,
      .display_name = info->display_name,
      .is_competing = info->is_competing,
  };
}

bool ServerCommandHandler::LeaveSession(const std::string& session_id, const std::string& player_id) {
  return session_manager_.Leave(session_id, player_id);
}

bool ServerCommandHandler::SubmitAnswer(const std::string& session_id,
                                        const std::string& player_id,
                                        const domain::PlayerAnswer& answer) {
  return session_manager_.SubmitAnswer(session_id, player_id, answer);
}

} // namespace quizlyx::server::app
