#include <QSignalSpy>
#include <QTest>

#include "model/SessionState.hpp"

using namespace quizlyx::client::model;

class TestSessionState : public QObject {
  Q_OBJECT
private slots:
  void phaseTransitionEmitsSignal();
  void addPlayerDeduplicates();
  void startQuestionSetsFields();
  void leaderboardSyncsPlayerScores();
};

void TestSessionState::phaseTransitionEmitsSignal() {
  SessionState s;
  QSignalSpy spy(&s, &SessionState::phaseChanged);
  s.setPhase(Phase::Connecting);
  s.setPhase(Phase::Connecting);
  s.setPhase(Phase::Connected);
  QCOMPARE(spy.count(), 2);
}

void TestSessionState::addPlayerDeduplicates() {
  SessionState s;
  PlayerEntry p1;
  p1.id = QStringLiteral("P1");
  p1.name = QStringLiteral("Alice");
  s.addPlayer(p1);
  s.addPlayer(p1);
  QCOMPARE(s.players().size(), 1);
}

void TestSessionState::startQuestionSetsFields() {
  SessionState s;
  QSignalSpy started(&s, &SessionState::questionStarted);
  QSignalSpy remaining(&s, &SessionState::remainingMsChanged);

  Question q;
  q.text = QStringLiteral("?");
  q.options = {QStringLiteral("A"), QStringLiteral("B")};
  q.correctIndices = {0};
  q.timeLimitMs = 5000;

  s.startQuestion(3, 10, 5000, q);
  QVERIFY(s.hasCurrentQuestion());
  QCOMPARE(s.currentQuestionIndex(), 3);
  QCOMPARE(s.totalQuestions(), 10);
  QCOMPARE(s.questionDurationMs(), 5000);
  QCOMPARE(s.remainingMs(), 5000);
  QCOMPARE(s.phase(), Phase::Question);
  QCOMPARE(started.count(), 1);
  QVERIFY(remaining.count() >= 1);
  QVERIFY(!s.hasAnswered());
}

void TestSessionState::leaderboardSyncsPlayerScores() {
  SessionState s;
  PlayerEntry p1;
  p1.id = QStringLiteral("P1");
  p1.score = 0;
  s.addPlayer(p1);
  PlayerEntry p2;
  p2.id = QStringLiteral("P2");
  p2.score = 0;
  s.addPlayer(p2);

  QVector<LeaderboardRow> rows;
  rows.push_back({QStringLiteral("P1"), 150});
  rows.push_back({QStringLiteral("P2"), 50});
  s.setLeaderboard(rows);

  QCOMPARE(s.players().at(0).score, 150);
  QCOMPARE(s.players().at(1).score, 50);
}

QTEST_MAIN(TestSessionState)
#include "test_session_state.moc"
