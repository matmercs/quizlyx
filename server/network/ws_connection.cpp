#include "ws_connection.hpp"

#include "game_controller.hpp"
#include "json_protocol.hpp"
#include "ws_connection_manager.hpp"

namespace quizlyx::server::network {

namespace {

nlohmann::json SerializePlayers(const domain::Session& session) {
  nlohmann::json players = nlohmann::json::array();
  for (const auto& player : session.players) {
    players.push_back({
        {"player_id", player.id},
        {"display_name", player.name},
        {"role", player.role == domain::Role::Host ? "host" : "player"},
        {"is_competing", player.is_competing},
        {"connected", player.connected},
    });
  }
  return players;
}

} // namespace

WsConnection::WsConnection(tcp::socket&& socket, GameController& controller, WsConnectionManager& manager) :
    ws_(std::move(socket)), controller_(controller), manager_(manager) {
  ws_.binary(false);
}

WsConnection::~WsConnection() {
  OnDisconnect();
}

void WsConnection::Start() {
  ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
  ws_.set_option(websocket::stream_base::decorator(
      [](websocket::response_type& res) { res.set(boost::beast::http::field::server, "quizlyx"); }));

  net::dispatch(ws_.get_executor(), beast::bind_front_handler(&WsConnection::DoAccept, shared_from_this()));
}

void WsConnection::Send(std::string message) {
  net::post(ws_.get_executor(), [self = shared_from_this(), msg = std::move(message)]() mutable {
    self->write_queue_.push(std::move(msg));
    if (!self->writing_) {
      self->DoWrite();
    }
  });
}

void WsConnection::DoAccept() {
  ws_.async_accept(beast::bind_front_handler([self = shared_from_this()](beast::error_code ec) {
    if (ec)
      return;
    self->DoRead();
  }));
}

void WsConnection::DoRead() {
  buffer_.consume(buffer_.size());
  ws_.async_read(buffer_, beast::bind_front_handler(&WsConnection::OnRead, shared_from_this()));
}

void WsConnection::OnRead(beast::error_code ec, std::size_t /*bytes_transferred*/) {
  if (ec) {
    OnDisconnect();
    return;
  }

  std::string text = beast::buffers_to_string(buffer_.data());
  HandleMessage(text);
  DoRead();
}

void WsConnection::HandleMessage(const std::string& text) {
  auto msg = ParseClientMessage(text);
  if (!msg) {
    Send(SerializeResponse("", false, {{"error", "invalid message"}}));
    return;
  }

  const auto& id = msg->id;
  const auto& type = msg->type;
  const auto& payload = msg->payload;

  try {
    if (type == "create_quiz") {
      auto quiz = DeserializeQuiz(payload);
      auto quiz_code = controller_.CreateQuiz(std::move(quiz));
      if (quiz_code) {
        Send(SerializeResponse(id, true, {{"quiz_code", *quiz_code}}));
      } else {
        Send(SerializeResponse(id, false, {{"error", "invalid quiz"}}));
      }
    } else if (type == "create_session") {
      auto quiz_code = payload.at("quiz_code").get<std::string>();
      auto host_name = payload.value("host_name", "Host");
      const bool host_is_spectator = payload.value("host_is_spectator", false);
      int auto_advance = payload.value("auto_advance_delay_ms", 0);
      auto result = controller_.CreateSession(quiz_code, host_name, host_is_spectator, auto_advance);
      if (result) {
        session_id_ = result->session_id;
        player_id_ = result->player_id;
        manager_.Register(session_id_, player_id_, shared_from_this());
        auto session = controller_.GetSessionById(session_id_);
        nlohmann::json response = {
            {"session_id", result->session_id},
            {"pin", result->pin},
            {"player_id", result->player_id},
            {"display_name", result->display_name},
            {"is_competing", result->is_competing},
        };
        if (session) {
          response["players"] = SerializePlayers(*session);
        }
        Send(SerializeResponse(id, true, response));
      } else {
        Send(SerializeResponse(id, false, {{"error", "failed to create session"}}));
      }
    } else if (type == "join") {
      auto pin = payload.at("pin").get<std::string>();
      auto name = payload.value("name", "");
      auto result = controller_.JoinAsPlayer(pin, name);
      if (result) {
        session_id_ = result->session_id;
        player_id_ = result->player_id;
        manager_.Register(session_id_, player_id_, shared_from_this());
        auto session = controller_.GetSessionById(session_id_);
        nlohmann::json response = {
            {"session_id", result->session_id},
            {"player_id", result->player_id},
            {"display_name", result->display_name},
            {"is_competing", result->is_competing},
        };
        if (session) {
          response["players"] = SerializePlayers(*session);
        }
        Send(SerializeResponse(id, true, response));
      } else {
        Send(SerializeResponse(id, false, {{"error", "failed to join"}}));
      }
    } else if (type == "start_game") {
      auto sid = payload.at("session_id").get<std::string>();
      bool ok = controller_.StartGame(sid);
      Send(SerializeResponse(id, ok, {{"ok", ok}}));
    } else if (type == "next_question") {
      auto sid = payload.at("session_id").get<std::string>();
      bool ok = controller_.NextQuestion(sid);
      Send(SerializeResponse(id, ok, {{"ok", ok}}));
    } else if (type == "leave") {
      auto sid = payload.at("session_id").get<std::string>();
      auto pid = payload.at("player_id").get<std::string>();
      bool ok = controller_.LeaveSession(sid, pid);
      if (ok) {
        manager_.Unregister(sid, pid);
        session_id_.clear();
        player_id_.clear();
      }
      Send(SerializeResponse(id, ok, {{"ok", ok}}));
    } else if (type == "submit_answer") {
      auto sid = payload.at("session_id").get<std::string>();
      auto pid = payload.at("player_id").get<std::string>();
      auto answer = DeserializePlayerAnswer(payload);
      bool ok = controller_.SubmitAnswer(sid, pid, answer);
      Send(SerializeResponse(id, ok, {{"ok", ok}}));
    } else if (type == "reconnect") {
      auto sid = payload.at("session_id").get<std::string>();
      auto pid = payload.at("player_id").get<std::string>();
      bool ok = controller_.Reconnect(sid, pid);
      if (ok) {
        session_id_ = sid;
        player_id_ = pid;
        manager_.Register(session_id_, player_id_, shared_from_this());
      }
      Send(SerializeResponse(id, ok, {{"ok", ok}}));
    } else {
      Send(SerializeResponse(id, false, {{"error", "unknown command"}}));
    }
  } catch (const std::exception& e) {
    Send(SerializeResponse(id, false, {{"error", e.what()}}));
  }
}

void WsConnection::DoWrite() {
  if (write_queue_.empty()) {
    writing_ = false;
    return;
  }

  writing_ = true;
  ws_.async_write(net::buffer(write_queue_.front()),
                  beast::bind_front_handler(&WsConnection::OnWrite, shared_from_this()));
}

void WsConnection::OnWrite(beast::error_code ec, std::size_t /*bytes_transferred*/) {
  if (ec) {
    OnDisconnect();
    return;
  }
  write_queue_.pop();
  DoWrite();
}

void WsConnection::OnDisconnect() {
  if (!session_id_.empty() && !player_id_.empty()) {
    std::string sid = std::move(session_id_);
    std::string pid = std::move(player_id_);
    session_id_.clear();
    player_id_.clear();
    manager_.Unregister(sid, pid);
    controller_.Disconnect(sid, pid);
  }
}

} // namespace quizlyx::server::network
