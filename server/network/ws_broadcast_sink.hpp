#ifndef QUIZLYX_SERVER_NETWORK_WS_BROADCAST_SINK_HPP
#define QUIZLYX_SERVER_NETWORK_WS_BROADCAST_SINK_HPP

#include "interfaces/ibroadcast_sink.hpp"
#include "network/ws_connection_manager.hpp"

namespace quizlyx::server::network {

class WsBroadcastSink : public interfaces::IBroadcastSink {
public:
  explicit WsBroadcastSink(WsConnectionManager& manager);
  void Broadcast(const std::string& session_id, const events::GameEvent& event) override;

private:
  WsConnectionManager& manager_;
};

} // namespace quizlyx::server::network

#endif // QUIZLYX_SERVER_NETWORK_WS_BROADCAST_SINK_HPP
