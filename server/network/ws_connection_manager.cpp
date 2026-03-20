#include "ws_connection_manager.hpp"

#include "ws_connection.hpp"

namespace quizlyx::server::network {

void WsConnectionManager::Register(const std::string& session_id,
                                   const std::string& player_id,
                                   const std::shared_ptr<WsConnection>& conn) {
  std::unique_lock lock(mutex_);
  connections_[session_id][player_id] = conn;
}

void WsConnectionManager::Unregister(const std::string& session_id, const std::string& player_id) {
  std::unique_lock lock(mutex_);
  auto it = connections_.find(session_id);
  if (it != connections_.end()) {
    it->second.erase(player_id);
    if (it->second.empty()) {
      connections_.erase(it);
    }
  }
}

std::vector<std::shared_ptr<WsConnection>> WsConnectionManager::GetSessionConnections(const std::string& session_id) {
  std::shared_lock lock(mutex_);
  std::vector<std::shared_ptr<WsConnection>> result;
  auto it = connections_.find(session_id);
  if (it != connections_.end()) {
    for (auto& [pid, weak_conn] : it->second) {
      if (auto conn = weak_conn.lock()) {
        result.push_back(conn);
      }
    }
  }
  return result;
}

} // namespace quizlyx::server::network
