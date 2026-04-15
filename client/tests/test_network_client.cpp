#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>
#include <QThread>
#include <QWebSocket>
#include <QWebSocketServer>

#include "model/Protocol.hpp"
#include "network/NetworkClient.hpp"

using namespace quizlyx::client;

class TestNetworkClient : public QObject {
  Q_OBJECT
private slots:
  void initTestCase();
  void connectsSendsAndReceivesResponse();
  void dispatchesQuestionStartedEvent();

private:
  std::unique_ptr<QWebSocketServer> fakeServer_;
  QList<QWebSocket*> fakeConnections_;
  quint16 fakePort_ = 0;

  void startFakeServer();
  void sendFakeFrame(const QByteArray& json);
};

void TestNetworkClient::initTestCase() {
  qRegisterMetaType<quizlyx::client::model::QuestionStartedEv>("quizlyx::client::model::QuestionStartedEv");
  qRegisterMetaType<quizlyx::client::model::TimerUpdateEv>("quizlyx::client::model::TimerUpdateEv");
  qRegisterMetaType<quizlyx::client::model::QuestionTimeoutEv>("quizlyx::client::model::QuestionTimeoutEv");
  qRegisterMetaType<quizlyx::client::model::AnswerRevealEv>("quizlyx::client::model::AnswerRevealEv");
  qRegisterMetaType<quizlyx::client::model::LeaderboardEv>("quizlyx::client::model::LeaderboardEv");
  qRegisterMetaType<quizlyx::client::model::PlayerJoinedEv>("quizlyx::client::model::PlayerJoinedEv");
  qRegisterMetaType<quizlyx::client::model::PlayerLeftEv>("quizlyx::client::model::PlayerLeftEv");
  qRegisterMetaType<quizlyx::client::model::GameFinishedEv>("quizlyx::client::model::GameFinishedEv");
}

void TestNetworkClient::startFakeServer() {
  fakeServer_ = std::make_unique<QWebSocketServer>(QStringLiteral("fake"), QWebSocketServer::NonSecureMode);
  QVERIFY(fakeServer_->listen(QHostAddress::LocalHost, 0));
  fakePort_ = fakeServer_->serverPort();
  connect(fakeServer_.get(), &QWebSocketServer::newConnection, this, [this]() {
    while (auto* ws = fakeServer_->nextPendingConnection()) {
      fakeConnections_.push_back(ws);
    }
  });
}

void TestNetworkClient::sendFakeFrame(const QByteArray& json) {
  for (auto* ws : fakeConnections_)
    ws->sendTextMessage(QString::fromUtf8(json));
}

void TestNetworkClient::connectsSendsAndReceivesResponse() {
  startFakeServer();

  QThread netThread;
  netThread.setObjectName(QStringLiteral("net-test"));
  auto* client = new network::NetworkClient;
  client->moveToThread(&netThread);
  connect(&netThread, &QThread::started, client, &network::NetworkClient::initialize);
  connect(&netThread, &QThread::finished, client, &QObject::deleteLater);
  netThread.start();

  QSignalSpy connectedSpy(client, &network::NetworkClient::connected);
  QSignalSpy responseSpy(client, &network::NetworkClient::responseReceived);

  QMetaObject::invokeMethod(client,
                            "connectTo",
                            Qt::QueuedConnection,
                            Q_ARG(QString, QStringLiteral("127.0.0.1")),
                            Q_ARG(quint16, fakePort_));
  QVERIFY(connectedSpy.wait(3000));

  for (auto* ws : fakeConnections_) {
    connect(ws, &QWebSocket::textMessageReceived, this, [ws](const QString& text) {
      const auto doc = QJsonDocument::fromJson(text.toUtf8());
      const auto obj = doc.object();
      QJsonObject resp{
          {QStringLiteral("id"), obj.value(QStringLiteral("id")).toString()},
          {QStringLiteral("type"), QStringLiteral("response")},
          {QStringLiteral("success"), true},
          {QStringLiteral("payload"), QJsonObject{{QStringLiteral("ok"), true}}},
      };
      ws->sendTextMessage(QString::fromUtf8(QJsonDocument(resp).toJson(QJsonDocument::Compact)));
    });
  }

  QMetaObject::invokeMethod(client,
                            "sendRequest",
                            Qt::QueuedConnection,
                            Q_ARG(QString, QStringLiteral("req-1")),
                            Q_ARG(QString, QStringLiteral("ping")),
                            Q_ARG(QJsonObject, QJsonObject{}));
  QVERIFY(responseSpy.wait(3000));
  QCOMPARE(responseSpy.first().at(0).toString(), QStringLiteral("req-1"));
  QCOMPARE(responseSpy.first().at(1).toBool(), true);

  netThread.quit();
  netThread.wait();
}

void TestNetworkClient::dispatchesQuestionStartedEvent() {
  startFakeServer();

  QThread netThread;
  auto* client = new network::NetworkClient;
  client->moveToThread(&netThread);
  connect(&netThread, &QThread::started, client, &network::NetworkClient::initialize);
  connect(&netThread, &QThread::finished, client, &QObject::deleteLater);
  netThread.start();

  QSignalSpy connectedSpy(client, &network::NetworkClient::connected);
  QSignalSpy qsSpy(client, &network::NetworkClient::questionStarted);

  QMetaObject::invokeMethod(client,
                            "connectTo",
                            Qt::QueuedConnection,
                            Q_ARG(QString, QStringLiteral("127.0.0.1")),
                            Q_ARG(quint16, fakePort_));
  QVERIFY(connectedSpy.wait(3000));

  const QJsonObject frame{
      {QStringLiteral("type"), QStringLiteral("event")},
      {QStringLiteral("event_type"), QStringLiteral("question_started")},
      {QStringLiteral("payload"),
       QJsonObject{{QStringLiteral("question_index"), 0},
                   {QStringLiteral("total_questions"), 3},
                   {QStringLiteral("duration_ms"), 7000},
                   {QStringLiteral("text"), QStringLiteral("Q?")},
                   {QStringLiteral("answer_type"), QStringLiteral("single_choice")},
                   {QStringLiteral("options"), QJsonArray{QStringLiteral("a"), QStringLiteral("b")}}}},
  };
  sendFakeFrame(QJsonDocument(frame).toJson(QJsonDocument::Compact));
  QVERIFY(qsSpy.wait(3000));

  netThread.quit();
  netThread.wait();
}

QTEST_MAIN(TestNetworkClient)
#include "test_network_client.moc"
