#include <QCoreApplication>
#include <QProcess>
#include <QSignalSpy>
#include <QTest>
#include <QThread>

#include "controller/GameController.hpp"
#include "model/SessionState.hpp"
#include "network/NetworkClient.hpp"

using namespace quizlyx::client;

class TestE2E : public QObject {
  Q_OBJECT
private slots:
  void initTestCase();
  void cleanupTestCase();
  void fullGameScenario();

private:
  QProcess server_;
  quint16 port_ = 8765;

  struct Peer {
    std::unique_ptr<QThread> thread;
    network::NetworkClient* net = nullptr;
    std::unique_ptr<model::SessionState> state;
    std::unique_ptr<controller::GameController> gc;
  };

  std::unique_ptr<Peer> makePeer();
  void teardownPeer(Peer& p);
};

void TestE2E::initTestCase() {
  qRegisterMetaType<quizlyx::client::model::QuestionStartedEv>("quizlyx::client::model::QuestionStartedEv");
  qRegisterMetaType<quizlyx::client::model::TimerUpdateEv>("quizlyx::client::model::TimerUpdateEv");
  qRegisterMetaType<quizlyx::client::model::QuestionTimeoutEv>("quizlyx::client::model::QuestionTimeoutEv");
  qRegisterMetaType<quizlyx::client::model::AnswerRevealEv>("quizlyx::client::model::AnswerRevealEv");
  qRegisterMetaType<quizlyx::client::model::LeaderboardEv>("quizlyx::client::model::LeaderboardEv");
  qRegisterMetaType<quizlyx::client::model::PlayerJoinedEv>("quizlyx::client::model::PlayerJoinedEv");
  qRegisterMetaType<quizlyx::client::model::PlayerLeftEv>("quizlyx::client::model::PlayerLeftEv");
  qRegisterMetaType<quizlyx::client::model::GameFinishedEv>("quizlyx::client::model::GameFinishedEv");
  qRegisterMetaType<quizlyx::client::model::Quiz>("quizlyx::client::model::Quiz");

  const auto serverPath = QStringLiteral("%1/../../server/Server").arg(QCoreApplication::applicationDirPath());
  server_.start(serverPath, {QString::number(port_)});
  QVERIFY2(server_.waitForStarted(5000), "Server binary failed to start");
  QTest::qWait(500);
}

void TestE2E::cleanupTestCase() {
  server_.terminate();
  if (!server_.waitForFinished(3000))
    server_.kill();
}

std::unique_ptr<TestE2E::Peer> TestE2E::makePeer() {
  auto peer = std::make_unique<Peer>();
  peer->thread = std::make_unique<QThread>();
  peer->net = new network::NetworkClient;
  peer->net->moveToThread(peer->thread.get());
  connect(peer->thread.get(), &QThread::started, peer->net, &network::NetworkClient::initialize);
  connect(peer->thread.get(), &QThread::finished, peer->net, &QObject::deleteLater);
  peer->thread->start();
  peer->state = std::make_unique<model::SessionState>();
  peer->gc = std::make_unique<controller::GameController>(peer->net, peer->state.get());
  return peer;
}

void TestE2E::teardownPeer(Peer& p) {
  if (p.thread) {
    p.thread->quit();
    p.thread->wait();
  }
}

void TestE2E::fullGameScenario() {
  auto host = makePeer();
  auto player = makePeer();

  QSignalSpy hostConnected(host->net, &network::NetworkClient::connected);
  host->gc->connectToServer(QStringLiteral("127.0.0.1"), port_);
  QVERIFY(hostConnected.wait(5000));

  model::Quiz quiz;
  quiz.title = QStringLiteral("E2E");
  quiz.description = QStringLiteral("integration");
  model::Question q;
  q.text = QStringLiteral("What is 2+2?");
  q.type = model::AnswerType::SingleChoice;
  q.options = {QStringLiteral("3"), QStringLiteral("4"), QStringLiteral("5")};
  q.correctIndices = {1};
  q.timeLimitMs = 800;
  quiz.questions.push_back(q);

  QSignalSpy sessionSpy(host->gc.get(), &controller::GameController::sessionCreated);
  host->gc->hostGame(quiz, QStringLiteral("host-player"), false, 0);
  QVERIFY(sessionSpy.wait(5000));
  const auto sessionId = host->state->sessionId();
  const auto pin = host->state->pin();
  QVERIFY(!sessionId.isEmpty());
  QCOMPARE(pin.size(), 6);

  QSignalSpy playerConnected(player->net, &network::NetworkClient::connected);
  player->gc->connectToServer(QStringLiteral("127.0.0.1"), port_);
  QVERIFY(playerConnected.wait(5000));

  QSignalSpy joinedSpy(player->gc.get(), &controller::GameController::joinedSession);
  player->gc->joinGame(pin, QStringLiteral("Alice"));
  QVERIFY(joinedSpy.wait(5000));
  QVERIFY(!player->state->myPlayerId().isEmpty());
  QCOMPARE(player->state->myDisplayName(), QStringLiteral("Alice"));

  QSignalSpy hostPlayers(host->state.get(), &model::SessionState::playersChanged);
  for (int i = 0; i < 50 && host->state->players().size() < 2; ++i) {
    if (hostPlayers.isEmpty())
      hostPlayers.wait(200);
    else
      QTest::qWait(50);
    hostPlayers.clear();
  }
  QVERIFY(host->state->players().size() >= 2);

  QSignalSpy hostQuestion(host->state.get(), &model::SessionState::questionStarted);
  QSignalSpy playerQuestion(player->state.get(), &model::SessionState::questionStarted);
  host->gc->startGame();
  QVERIFY(hostQuestion.wait(5000));
  if (playerQuestion.isEmpty())
    QVERIFY(playerQuestion.wait(5000));

  QVERIFY(player->state->hasCurrentQuestion());
  QCOMPARE(player->state->currentQuestion().text, QStringLiteral("What is 2+2?"));
  QCOMPARE(player->state->currentQuestion().options.size(), 3);
  QCOMPARE(player->state->totalQuestions(), 1);

  QSignalSpy hostReveal(host->state.get(), &model::SessionState::answerRevealStarted);
  QSignalSpy hostFinished(host->state.get(), &model::SessionState::gameFinished);
  player->gc->submitAnswer({1});
  QVERIFY(hostReveal.wait(5000));
  QVERIFY(hostFinished.wait(7000));
  QCOMPARE(host->state->phase(), model::Phase::Finished);
  QVERIFY(!host->state->leaderboard().isEmpty());

  int playerScore = 0;
  for (const auto& row : host->state->leaderboard()) {
    if (row.playerId == player->state->myPlayerId())
      playerScore = row.score;
  }
  QVERIFY(playerScore > 0);

  teardownPeer(*host);
  teardownPeer(*player);
}

QTEST_MAIN(TestE2E)
#include "test_e2e.moc"
