#include "ws_server.hpp"

#include "game_controller.hpp"
#include "ws_connection.hpp"
#include "ws_connection_manager.hpp"

#include <boost/asio/strand.hpp>
#include <iostream>
#include <stdexcept>

namespace quizlyx::server::network {

WsServer::WsServer(net::io_context& ioc,
                   const tcp::endpoint& endpoint,
                   GameController& controller,
                   WsConnectionManager& manager) :
    ioc_(ioc), acceptor_(net::make_strand(ioc)), controller_(controller), manager_(manager) {
  beast::error_code ec;

  acceptor_.open(endpoint.protocol(), ec); // NOLINT(bugprone-unused-return-value)
  if (ec)
    throw std::runtime_error("Failed to open acceptor: " + ec.message());

  acceptor_.set_option(net::socket_base::reuse_address(true), ec); // NOLINT(bugprone-unused-return-value)
  if (ec)
    throw std::runtime_error("Failed to set reuse_address: " + ec.message());

  acceptor_.bind(endpoint, ec); // NOLINT(bugprone-unused-return-value)
  if (ec)
    throw std::runtime_error("Failed to bind: " + ec.message());

  acceptor_.listen(net::socket_base::max_listen_connections, ec); // NOLINT(bugprone-unused-return-value)
  if (ec)
    throw std::runtime_error("Failed to listen: " + ec.message());
}

void WsServer::Start() {
  DoAccept();
}

void WsServer::Stop() {
  acceptor_.close();
}

void WsServer::DoAccept() {
  acceptor_.async_accept(net::make_strand(ioc_), [this](beast::error_code ec, tcp::socket socket) {
    if (ec)
      return;
    auto conn = std::make_shared<WsConnection>(std::move(socket), controller_, manager_);
    conn->Start();
    DoAccept();
  });
}

} // namespace quizlyx::server::network
