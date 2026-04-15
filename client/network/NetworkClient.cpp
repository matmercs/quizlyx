#include "network/NetworkClient.hpp"

#include <QJsonDocument>
#include <QUrl>
#include <QWebSocket>

namespace quizlyx::client::network {

NetworkClient::NetworkClient(QObject* parent) : QObject(parent) {
}

NetworkClient::~NetworkClient() {
  if (socket_ != nullptr) {
    socket_->close();
  }
}

void NetworkClient::initialize() {
  if (socket_ != nullptr)
    return;

  socket_ = new QWebSocket(QStringLiteral("quizlyx-client"), QWebSocketProtocol::VersionLatest, this);
  connect(socket_, &QWebSocket::connected, this, &NetworkClient::onConnected);
  connect(socket_, &QWebSocket::disconnected, this, &NetworkClient::onDisconnected);
  connect(socket_, &QWebSocket::textMessageReceived, this, &NetworkClient::onTextMessageReceived);
  connect(socket_, &QWebSocket::errorOccurred, this, &NetworkClient::onSocketError);
}

void NetworkClient::connectTo(QString host, quint16 port) {
  if (socket_ == nullptr)
    initialize();
  if (socket_->state() == QAbstractSocket::ConnectedState || socket_->state() == QAbstractSocket::ConnectingState) {
    socket_->close();
  }
  QUrl url;
  url.setScheme(QStringLiteral("ws"));
  url.setHost(host);
  url.setPort(port);
  url.setPath(QStringLiteral("/"));
  socket_->open(url);
}

void NetworkClient::disconnectFromServer() {
  if (socket_ == nullptr)
    return;
  socket_->close();
}

void NetworkClient::sendRequest(QString requestId, QString type, QJsonObject payload) {
  if (socket_ == nullptr || socket_->state() != QAbstractSocket::ConnectedState) {
    emit errorOccurred(QStringLiteral("not connected"));
    emit responseReceived(requestId, false, QJsonObject{{QStringLiteral("error"), QStringLiteral("not connected")}});
    return;
  }
  const auto msg = model::makeCommand(requestId, type, payload);
  const auto bytes = QJsonDocument(msg).toJson(QJsonDocument::Compact);
  socket_->sendTextMessage(QString::fromUtf8(bytes));
}

void NetworkClient::onConnected() {
  emit connected();
}

void NetworkClient::onDisconnected() {
  emit disconnected();
}

void NetworkClient::onSocketError(QAbstractSocket::SocketError) {
  if (socket_ != nullptr)
    emit errorOccurred(socket_->errorString());
}

void NetworkClient::onTextMessageReceived(const QString& text) {
  const auto frame = model::parseServerFrame(text.toUtf8());
  switch (frame.kind) {
    case model::ServerFrame::Kind::Response:
      emit responseReceived(frame.id, frame.success, frame.payload);
      break;
    case model::ServerFrame::Kind::Event:
      if (frame.hasEvent)
        dispatchEvent(frame.event);
      break;
    case model::ServerFrame::Kind::Invalid:
      emit errorOccurred(QStringLiteral("invalid frame from server"));
      break;
  }
}

void NetworkClient::dispatchEvent(const model::ServerEvent& event) {
  std::visit(
      [this](auto&& ev) {
        using T = std::decay_t<decltype(ev)>;
        if constexpr (std::is_same_v<T, model::QuestionStartedEv>) {
          emit questionStarted(ev);
        } else if constexpr (std::is_same_v<T, model::TimerUpdateEv>) {
          emit timerUpdate(ev);
        } else if constexpr (std::is_same_v<T, model::QuestionTimeoutEv>) {
          emit questionTimedOut();
        } else if constexpr (std::is_same_v<T, model::AnswerRevealEv>) {
          emit answerReveal(ev);
        } else if constexpr (std::is_same_v<T, model::LeaderboardEv>) {
          emit leaderboardUpdated(ev);
        } else if constexpr (std::is_same_v<T, model::PlayerJoinedEv>) {
          emit playerJoined(ev);
        } else if constexpr (std::is_same_v<T, model::PlayerLeftEv>) {
          emit playerLeft(ev);
        } else if constexpr (std::is_same_v<T, model::GameFinishedEv>) {
          emit gameFinished(ev);
        }
      },
      event);
}

} // namespace quizlyx::client::network
