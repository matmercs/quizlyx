#ifndef QUIZLYX_SERVER_SERVICES_SESSION_MANAGER_HPP
#define QUIZLYX_SERVER_SERVICES_SESSION_MANAGER_HPP

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "domain/answer.hpp"
#include "domain/session.hpp"
#include "events/game_events.hpp"
#include "interfaces/ibroadcast_sink.hpp"
#include "interfaces/itime_provider.hpp"
#include "services/quiz_registry.hpp"
#include "services/scoring.hpp"

namespace quizlyx::server::services {

class SessionManager {
public:
  struct SessionInfo {
    std::string session_id;
    std::string pin;
    std::string player_id;
    std::string display_name;
    bool is_competing = true;
  };

  struct JoinInfo {
    std::string session_id;
    std::string player_id;
    std::string display_name;
    bool is_competing = true;
  };

  SessionManager(QuizRegistry& quiz_registry,
                 interfaces::IBroadcastSink& broadcast_sink,
                 interfaces::ITimeProvider& time_provider);

  std::optional<SessionInfo> CreateSession(const std::string& quiz_code,
                                           const std::string& host_player_id,
                                           const std::string& host_display_name,
                                           bool host_is_spectator,
                                           int auto_advance_delay_ms);

  std::optional<domain::Session> GetSessionById(const std::string& session_id) const;

  std::optional<JoinInfo> JoinByPin(const std::string& pin,
                                    const std::string& player_id,
                                    const std::string& display_name);
  bool Leave(const std::string& session_id, const std::string& player_id);

  bool StartGame(const std::string& session_id);
  bool NextQuestion(const std::string& session_id);
  std::optional<events::GameEvent> CompleteQuestion(const std::string& session_id);
  std::optional<events::GameEvent> FinishReveal(const std::string& session_id);

  bool SubmitAnswer(const std::string& session_id, const std::string& player_id, const domain::PlayerAnswer& answer);

  bool Disconnect(const std::string& session_id, const std::string& player_id);
  bool Reconnect(const std::string& session_id, const std::string& player_id);
  std::vector<std::pair<std::string, std::string>> CleanupDisconnectedPlayers(std::chrono::milliseconds timeout);

private:
  struct SessionEntry {
    domain::Session session;
    std::optional<events::GameEvent> pending_post_reveal_event;
    mutable std::mutex mutex;
  };

  QuizRegistry& quiz_registry_;
  interfaces::IBroadcastSink& broadcast_sink_;
  interfaces::ITimeProvider& time_provider_;

  mutable std::mutex global_mutex_;
  std::unordered_map<std::string, std::unique_ptr<SessionEntry>> sessions_;
  size_t next_session_id_ = 0;

  SessionEntry* FindEntry(const std::string& session_id) const;
  SessionEntry* FindEntryByPin(const std::string& pin) const;
  static std::string ResolveDisplayName(const domain::Session& session, const std::string& requested_display_name);
  std::string GenerateSessionId();
  std::string GeneratePin() const;
};

} // namespace quizlyx::server::services

#endif // QUIZLYX_SERVER_SERVICES_SESSION_MANAGER_HPP
