#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>
#include <QThread>
#include <QWebSocket>
#include <QWebSocketServer>

#include "controller/GameController.hpp"
#include "model/SessionState.hpp"
#include "network/NetworkClient.hpp"

using namespace quizlyx::client;

class TestGameController : public QObject {
  Q_OBJECT
private slots:
  void initTestCase();
  void hostFlowUpdatesState();

private:
  static QJsonObject response(const QString& id, bool success, const QJsonObject& payload);
  static QJsonObject event(const QString& type, const QJsonObject& payload);
};

void TestGameController::initTestCase() {
  qRegisterMetaType<quizlyx::client::model::QuestionStartedEv>("quizlyx::client::model::QuestionStartedEv");
  qRegisterMetaType<quizlyx::client::model::TimerUpdateEv>("quizlyx::client::model::TimerUpdateEv");
  qRegisterMetaType<quizlyx::client::model::QuestionTimeoutEv>("quizlyx::client::model::QuestionTimeoutEv");
  qRegisterMetaType<quizlyx::client::model::AnswerRevealEv>("quizlyx::client::model::AnswerRevealEv");
  qRegisterMetaType<quizlyx::client::model::LeaderboardEv>("quizlyx::client::model::LeaderboardEv");
  qRegisterMetaType<quizlyx::client::model::PlayerJoinedEv>("quizlyx::client::model::PlayerJoinedEv");
  qRegisterMetaType<quizlyx::client::model::PlayerLeftEv>("quizlyx::client::model::PlayerLeftEv");
  qRegisterMetaType<quizlyx::client::model::GameFinishedEv>("quizlyx::client::model::GameFinishedEv");
  qRegisterMetaType<quizlyx::client::model::Quiz>("quizlyx::client::model::Quiz");
}

QJsonObject TestGameController::response(const QString& id, bool success, const QJsonObject& payload) {
  return QJsonObject{
      {QStringLiteral("id"), id},
      {QStringLiteral("type"), QStringLiteral("response")},
      {QStringLiteral("success"), success},
      {QStringLiteral("payload"), payload},
  };
}

QJsonObject TestGameController::event(const QString& type, const QJsonObject& payload) {
  return QJsonObject{
      {QStringLiteral("type"), QStringLiteral("event")},
      {QStringLiteral("event_type"), type},
      {QStringLiteral("payload"), payload},
  };
}

void TestGameController::hostFlowUpdatesState() {
  QWebSocketServer server(QStringLiteral("fake"), QWebSocketServer::NonSecureMode);
  QVERIFY(server.listen(QHostAddress::LocalHost, 0));
  const auto port = server.serverPort();

  QObject::connect(&server, &QWebSocketServer::newConnection, this, [this, &server]() {
    auto* sock = server.nextPendingConnection();
    if (sock == nullptr)
      return;
    connect(sock, &QWebSocket::textMessageReceived, this, [sock, this](const QString& text) {
      const auto obj = QJsonDocument::fromJson(text.toUtf8()).object();
      const auto type = obj.value(QStringLiteral("type")).toString();
      const auto id = obj.value(QStringLiteral("id")).toString();
      if (type == QStringLiteral("create_quiz")) {
        sock->sendTextMessage(
            QJsonDocument(response(id, true, QJsonObject{{QStringLiteral("quiz_code"), QStringLiteral("Q0")}}))
                .toJson(QJsonDocument::Compact));
      } else if (type == QStringLiteral("create_session")) {
        sock->sendTextMessage(
            QJsonDocument(response(id,
                                   true,
                                   QJsonObject{
                                       {QStringLiteral("session_id"), QStringLiteral("S1")},
                                       {QStringLiteral("pin"), QStringLiteral("123456")},
                                       {QStringLiteral("player_id"), QStringLiteral("host-1")},
                                       {QStringLiteral("display_name"), QStringLiteral("Host")},
                                       {QStringLiteral("is_competing"), true},
                                       {QStringLiteral("players"),
                                        QJsonArray{QJsonObject{{QStringLiteral("player_id"), QStringLiteral("host-1")},
                                                               {QStringLiteral("display_name"), QStringLiteral("Host")},
                                                               {QStringLiteral("role"), QStringLiteral("host")},
                                                               {QStringLiteral("is_competing"), true},
                                                               {QStringLiteral("connected"), true}}}},
                                   }))
                .toJson(QJsonDocument::Compact));
      }
    });
  });

  QThread netThread;
  auto* netClient = new network::NetworkClient;
  netClient->moveToThread(&netThread);
  connect(&netThread, &QThread::started, netClient, &network::NetworkClient::initialize);
  connect(&netThread, &QThread::finished, netClient, &QObject::deleteLater);
  netThread.start();

  model::SessionState state;
  controller::GameController gc(netClient, &state);
  QSignalSpy sessionSpy(&gc, &controller::GameController::sessionCreated);
  QSignalSpy phaseSpy(&state, &model::SessionState::phaseChanged);
  QSignalSpy playersSpy(&state, &model::SessionState::playersChanged);

  QSignalSpy netConnected(netClient, &network::NetworkClient::connected);

  gc.connectToServer(QStringLiteral("127.0.0.1"), port);
  QVERIFY(netConnected.wait(3000));

  model::Quiz quiz;
  quiz.title = QStringLiteral("T");
  model::Question q;
  q.text = QStringLiteral("2+2?");
  q.options = {QStringLiteral("3"), QStringLiteral("4")};
  q.correctIndices = {1};
  q.timeLimitMs = 5000;
  quiz.questions.push_back(q);

  gc.hostGame(quiz, QStringLiteral("host"), false, 0);

  QVERIFY(sessionSpy.wait(3000));
  QCOMPARE(state.sessionId(), QStringLiteral("S1"));
  QCOMPARE(state.pin(), QStringLiteral("123456"));
  QCOMPARE(state.myPlayerId(), QStringLiteral("host-1"));
  QCOMPARE(state.myDisplayName(), QStringLiteral("Host"));
  QCOMPARE(state.role(), model::Role::Host);
  QCOMPARE(state.phase(), model::Phase::Lobby);
  if (playersSpy.isEmpty())
    QVERIFY(playersSpy.wait(3000));
  QVERIFY(playersSpy.count() >= 1);
  QCOMPARE(state.players().size(), 1);
  QCOMPARE(state.players().first().name, QStringLiteral("Host"));
  QVERIFY(state.players().first().isHost);
  QVERIFY(state.players().first().isCompeting);

  netThread.quit();
  netThread.wait();
}

QTEST_MAIN(TestGameController)
#include "test_game_controller.moc"
