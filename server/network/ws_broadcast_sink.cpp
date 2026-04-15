#include "ws_broadcast_sink.hpp"

#include "json_protocol.hpp"
#include "ws_connection.hpp"

namespace quizlyx::server::network {

WsBroadcastSink::WsBroadcastSink(WsConnectionManager& manager) : manager_(manager) {
}

void WsBroadcastSink::Broadcast(const std::string& session_id, const events::GameEvent& event) {
  auto connections = manager_.GetSessionConnections(session_id);
  for (auto& conn : connections) {
    conn->Send(SerializeGameEvent(event, conn->playerId()));
  }
}

} // namespace quizlyx::server::network
