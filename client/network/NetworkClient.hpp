#ifndef QUIZLYX_CLIENT_NETWORK_NETWORK_CLIENT_HPP
#define QUIZLYX_CLIENT_NETWORK_NETWORK_CLIENT_HPP

#include <QAbstractSocket>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include "model/Protocol.hpp"

class QWebSocket;

namespace quizlyx::client::network {

class NetworkClient : public QObject {
  Q_OBJECT
public:
  explicit NetworkClient(QObject* parent = nullptr);
  ~NetworkClient() override;

public slots:
  void initialize();
  void connectTo(QString host, quint16 port);
  void disconnectFromServer();
  void sendRequest(QString requestId, QString type, QJsonObject payload);

signals:
  void connected();
  void disconnected();
  void errorOccurred(QString message);
  void responseReceived(QString requestId, bool success, QJsonObject payload);

  void questionStarted(quizlyx::client::model::QuestionStartedEv ev);
  void timerUpdate(quizlyx::client::model::TimerUpdateEv ev);
  void questionTimedOut();
  void answerReveal(quizlyx::client::model::AnswerRevealEv ev);
  void leaderboardUpdated(quizlyx::client::model::LeaderboardEv ev);
  void playerJoined(quizlyx::client::model::PlayerJoinedEv ev);
  void playerLeft(quizlyx::client::model::PlayerLeftEv ev);
  void gameFinished(quizlyx::client::model::GameFinishedEv ev);

private slots:
  void onConnected();
  void onDisconnected();
  void onTextMessageReceived(const QString& text);
  void onSocketError(QAbstractSocket::SocketError error);

private:
  void dispatchEvent(const model::ServerEvent& event);

  QWebSocket* socket_ = nullptr;
};

} // namespace quizlyx::client::network

#endif // QUIZLYX_CLIENT_NETWORK_NETWORK_CLIENT_HPP
