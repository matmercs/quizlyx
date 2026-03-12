#ifndef QUIZLYX_SERVER_SERVICES_SESSION_MANAGER_HPP
#define QUIZLYX_SERVER_SERVICES_SESSION_MANAGER_HPP

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "domain/answer.hpp"
#include "domain/session.hpp"
#include "events/game_events.hpp"
#include "interfaces/ibroadcast_sink.hpp"
#include "services/quiz_registry.hpp"
#include "services/scoring.hpp"

namespace quizlyx::server::services {

class SessionManager {
public:
  struct SessionInfo {
    std::string session_id;
    std::string pin;
  };

  SessionManager(QuizRegistry& quiz_registry, interfaces::IBroadcastSink& broadcast_sink);

  std::optional<SessionInfo> CreateSession(const std::string& quiz_code, const std::string& host_id);

  std::optional<domain::Session> GetSessionById(const std::string& session_id) const;
  std::optional<domain::Session> GetSessionByPin(const std::string& pin) const;

  bool JoinAsPlayer(const std::string& pin, const std::string& player_id);
  bool Leave(const std::string& session_id, const std::string& player_id);

  bool StartGame(const std::string& session_id);
  bool NextQuestion(const std::string& session_id);

  bool SubmitAnswer(const std::string& session_id, const std::string& player_id, const domain::PlayerAnswer& answer);

private:
  QuizRegistry& quiz_registry_;
  interfaces::IBroadcastSink& broadcast_sink_;

  mutable std::mutex mutex_;
  std::unordered_map<std::string, domain::Session> sessions_by_id_;
  std::unordered_map<std::string, std::string> session_id_by_pin_;
  size_t next_session_id_ = 0;

  std::string GenerateSessionId();
  std::string GeneratePin() const;
};

} // namespace quizlyx::server::services

#endif // QUIZLYX_SERVER_SERVICES_SESSION_MANAGER_HPP
