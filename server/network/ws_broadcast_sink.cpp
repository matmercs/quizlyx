#include "ws_broadcast_sink.hpp"

#include "json_protocol.hpp"
#include "ws_connection.hpp"

namespace quizlyx::server::network {

WsBroadcastSink::WsBroadcastSink(WsConnectionManager& manager) : manager_(manager) {
}

void WsBroadcastSink::Broadcast(const std::string& session_id, const events::GameEvent& event) {
  std::string message = SerializeGameEvent(event);
  auto connections = manager_.GetSessionConnections(session_id);
  for (auto& conn : connections) {
    conn->Send(message);
  }
}

} // namespace quizlyx::server::network
