#ifndef QUIZLYX_SERVER_NETWORK_WS_CONNECTION_HPP
#define QUIZLYX_SERVER_NETWORK_WS_CONNECTION_HPP

#include <boost/asio/dispatch.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <memory>
#include <queue>
#include <string>

namespace quizlyx::server::network {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class GameController;
class WsConnectionManager;

class WsConnection : public std::enable_shared_from_this<WsConnection> {
public:
  WsConnection(tcp::socket&& socket, GameController& controller, WsConnectionManager& manager);
  ~WsConnection();

  void Start();
  void Send(std::string message);
  const std::string& playerId() const {
    return player_id_;
  }

private:
  void DoAccept();
  void DoRead();
  void OnRead(beast::error_code ec, std::size_t bytes_transferred);
  void HandleMessage(const std::string& text);
  void DoWrite();
  void OnWrite(beast::error_code ec, std::size_t bytes_transferred);
  void OnDisconnect();

  websocket::stream<beast::tcp_stream> ws_;
  beast::flat_buffer buffer_;

  GameController& controller_;
  WsConnectionManager& manager_;

  std::queue<std::string> write_queue_;
  bool writing_ = false;

  std::string session_id_;
  std::string player_id_;
};

} // namespace quizlyx::server::network

#endif // QUIZLYX_SERVER_NETWORK_WS_CONNECTION_HPP
