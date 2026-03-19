#ifndef QUIZLYX_SERVER_INTERFACES_ICOMMAND_HANDLER_HPP
#define QUIZLYX_SERVER_INTERFACES_ICOMMAND_HANDLER_HPP

#include <optional>
#include <string>

#include "domain/answer.hpp"
#include "domain/quiz.hpp"

namespace quizlyx::server::interfaces {

struct SessionCreated {
  std::string session_id;
  std::string pin;
};

class ICommandHandler {
public:
  virtual ~ICommandHandler() = default;

  virtual std::optional<std::string> CreateQuiz(domain::Quiz quiz) = 0;

  virtual std::optional<SessionCreated> CreateSession(const std::string& quiz_code,
                                                      const std::string& host_id,
                                                      int auto_advance_delay_ms) = 0;

  virtual bool StartGame(const std::string& session_id) = 0;
  virtual bool NextQuestion(const std::string& session_id) = 0;

  virtual bool JoinAsPlayer(const std::string& session_id,
                            const std::string& pin,
                            const std::string& player_id,
                            const std::string& display_name) = 0;
  virtual bool LeaveSession(const std::string& session_id, const std::string& player_id) = 0;

  virtual bool SubmitAnswer(const std::string& session_id,
                            const std::string& player_id,
                            const domain::PlayerAnswer& answer) = 0;
};

} // namespace quizlyx::server::interfaces

#endif // QUIZLYX_SERVER_INTERFACES_ICOMMAND_HANDLER_HPP
