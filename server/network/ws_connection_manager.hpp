#ifndef QUIZLYX_SERVER_NETWORK_WS_CONNECTION_MANAGER_HPP
#define QUIZLYX_SERVER_NETWORK_WS_CONNECTION_MANAGER_HPP

#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace quizlyx::server::network {

class WsConnection;

class WsConnectionManager {
public:
  void Register(const std::string& session_id, const std::string& player_id, const std::shared_ptr<WsConnection>& conn);
  void Unregister(const std::string& session_id, const std::string& player_id);
  std::vector<std::shared_ptr<WsConnection>> GetSessionConnections(const std::string& session_id);

private:
  mutable std::shared_mutex mutex_;
  std::unordered_map<std::string, std::unordered_map<std::string, std::weak_ptr<WsConnection>>> connections_;
};

} // namespace quizlyx::server::network

#endif // QUIZLYX_SERVER_NETWORK_WS_CONNECTION_MANAGER_HPP
