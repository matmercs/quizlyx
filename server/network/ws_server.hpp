#ifndef QUIZLYX_SERVER_NETWORK_WS_SERVER_HPP
#define QUIZLYX_SERVER_NETWORK_WS_SERVER_HPP

#include <boost/beast/core.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace quizlyx::server::network {

namespace beast = boost::beast;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class GameController;
class WsConnectionManager;

class WsServer {
public:
  WsServer(net::io_context& ioc, tcp::endpoint endpoint,
           GameController& controller, WsConnectionManager& manager);

  void Start();
  void Stop();
  uint16_t Port() const { return acceptor_.local_endpoint().port(); }

private:
  void DoAccept();

  net::io_context& ioc_;
  tcp::acceptor acceptor_;
  GameController& controller_;
  WsConnectionManager& manager_;
};

} // namespace quizlyx::server::network

#endif // QUIZLYX_SERVER_NETWORK_WS_SERVER_HPP
