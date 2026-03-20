#include "game_controller.hpp"

namespace quizlyx::server::network {

GameController::GameController(interfaces::ICommandHandler& handler,
                               services::SessionTimerService& timer_service,
                               services::SessionManager& session_manager) :
    handler_(handler), timer_service_(timer_service), session_manager_(session_manager) {
}

std::optional<std::string> GameController::CreateQuiz(domain::Quiz quiz) {
  return handler_.CreateQuiz(std::move(quiz));
}

std::optional<interfaces::SessionCreated> GameController::CreateSession(const std::string& quiz_code,
                                                                        const std::string& host_id,
                                                                        int auto_advance_delay_ms) {
  return handler_.CreateSession(quiz_code, host_id, auto_advance_delay_ms);
}

bool GameController::StartGame(const std::string& session_id) {
  bool ok = handler_.StartGame(session_id);
  if (ok) {
    auto session = session_manager_.GetSessionById(session_id);
    if (session && session->has_question_deadline) {
      timer_service_.SetDeadline(session_id, session->question_deadline);
    }
  }
  return ok;
}

bool GameController::NextQuestion(const std::string& session_id) {
  timer_service_.ClearDeadline(session_id);
  bool ok = handler_.NextQuestion(session_id);
  if (ok) {
    auto session = session_manager_.GetSessionById(session_id);
    if (session && session->has_question_deadline) {
      timer_service_.SetDeadline(session_id, session->question_deadline);
    }
  }
  return ok;
}

std::optional<std::string> GameController::JoinAsPlayer(const std::string& session_id,
                                                        const std::string& pin,
                                                        const std::string& display_name) {
  std::string player_id = GeneratePlayerId();
  std::string name = display_name.empty() ? player_id : display_name;
  bool ok = handler_.JoinAsPlayer(session_id, pin, player_id, name);
  if (ok)
    return player_id;
  return std::nullopt;
}

bool GameController::LeaveSession(const std::string& session_id, const std::string& player_id) {
  return handler_.LeaveSession(session_id, player_id);
}

bool GameController::SubmitAnswer(const std::string& session_id,
                                  const std::string& player_id,
                                  const domain::PlayerAnswer& answer) {
  return handler_.SubmitAnswer(session_id, player_id, answer);
}

bool GameController::Disconnect(const std::string& session_id, const std::string& player_id) {
  return session_manager_.Disconnect(session_id, player_id);
}

bool GameController::Reconnect(const std::string& session_id, const std::string& player_id) {
  return session_manager_.Reconnect(session_id, player_id);
}

std::string GameController::GeneratePlayerId() {
  return "P" + std::to_string(next_player_id_.fetch_add(1));
}

} // namespace quizlyx::server::network
