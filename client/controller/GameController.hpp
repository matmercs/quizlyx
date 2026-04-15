#ifndef QUIZLYX_CLIENT_CONTROLLER_GAME_CONTROLLER_HPP
#define QUIZLYX_CLIENT_CONTROLLER_GAME_CONTROLLER_HPP

#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>
#include <functional>

#include "model/Protocol.hpp"
#include "model/Quiz.hpp"
#include "model/SessionState.hpp"

namespace quizlyx::client::network {
class NetworkClient;
}

namespace quizlyx::client::controller {

class GameController : public QObject {
  Q_OBJECT
public:
  GameController(network::NetworkClient* net, model::SessionState* state, QObject* parent = nullptr);

public slots:
  void connectToServer(QString host, quint16 port);
  void disconnectFromServer();
  void hostGame(model::Quiz quiz, QString hostName, bool hostIsSpectator, int autoAdvanceDelayMs = 0);
  void joinGame(QString pin, QString name);
  void startGame();
  void submitAnswer(QVector<int> selected);
  void requestNextQuestion();
  void leaveSession();

signals:
  void errorOccurred(QString message);
  void sessionCreated(QString sessionId, QString pin);
  void joinedSession(QString sessionId, QString playerId);
  void connectionFailed(QString message);

  void requestConnect(QString host, quint16 port);
  void requestDisconnect();
  void requestSend(QString requestId, QString type, QJsonObject payload);

private slots:
  void onConnected();
  void onDisconnected();
  void onNetworkError(QString message);
  void onResponseReceived(QString id, bool success, QJsonObject payload);

  void onQuestionStarted(quizlyx::client::model::QuestionStartedEv ev);
  void onTimerUpdate(quizlyx::client::model::TimerUpdateEv ev);
  void onQuestionTimedOut();
  void onAnswerReveal(quizlyx::client::model::AnswerRevealEv ev);
  void onLeaderboardUpdated(quizlyx::client::model::LeaderboardEv ev);
  void onPlayerJoined(quizlyx::client::model::PlayerJoinedEv ev);
  void onPlayerLeft(quizlyx::client::model::PlayerLeftEv ev);
  void onGameFinished(quizlyx::client::model::GameFinishedEv ev);

private:
  using ResponseHandler = std::function<void(bool, const QJsonObject&)>;

  QString nextRequestId();
  void sendRequest(const QString& type, const QJsonObject& payload, ResponseHandler handler);

  network::NetworkClient* net_;
  model::SessionState* state_;
  QHash<QString, ResponseHandler> pending_;
  quint64 nextId_ = 1;

  QElapsedTimer questionElapsed_;
  bool questionElapsedValid_ = false;

  model::Quiz stagedQuiz_;
  QString stagedHostId_;
  bool stagedHostIsSpectator_ = false;
  int stagedAutoAdvanceMs_ = 0;

  QString stagedJoinName_;
};

} // namespace quizlyx::client::controller

#endif // QUIZLYX_CLIENT_CONTROLLER_GAME_CONTROLLER_HPP
