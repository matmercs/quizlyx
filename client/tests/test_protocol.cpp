#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "model/Protocol.hpp"

using namespace quizlyx::client::model;

class TestProtocol : public QObject {
  Q_OBJECT
private slots:
  void quizRoundTrip();
  void makeCommandWrapsFields();
  void parsesResponseFrame();
  void parsesQuestionStartedEvent();
  void parsesTimerUpdateEvent();
  void parsesAnswerRevealEvent();
  void parsesLeaderboardEvent();
  void parsesPlayerJoinedEvent();
  void parsesGameFinishedEvent();
};

void TestProtocol::quizRoundTrip() {
  Quiz in;
  in.title = QStringLiteral("Math");
  in.description = QStringLiteral("arithmetic");
  Question q1;
  q1.text = QStringLiteral("2+2?");
  q1.type = AnswerType::SingleChoice;
  q1.options = {QStringLiteral("3"), QStringLiteral("4"), QStringLiteral("5")};
  q1.correctIndices = {1};
  q1.timeLimitMs = 5000;
  in.questions.push_back(q1);

  const auto json = toJson(in);
  const auto out = quizFromJson(json);

  QCOMPARE(out.title, in.title);
  QCOMPARE(out.description, in.description);
  QCOMPARE(out.questions.size(), in.questions.size());
  QCOMPARE(out.questions.first().text, q1.text);
  QCOMPARE(out.questions.first().options, q1.options);
  QCOMPARE(out.questions.first().correctIndices, q1.correctIndices);
  QCOMPARE(out.questions.first().timeLimitMs, q1.timeLimitMs);
  QCOMPARE(out.questions.first().type, q1.type);
}

void TestProtocol::makeCommandWrapsFields() {
  const auto obj = makeCommand(
      QStringLiteral("1"), QStringLiteral("join"), QJsonObject{{QStringLiteral("pin"), QStringLiteral("123456")}});
  QCOMPARE(obj.value(QStringLiteral("id")).toString(), QStringLiteral("1"));
  QCOMPARE(obj.value(QStringLiteral("type")).toString(), QStringLiteral("join"));
  QCOMPARE(obj.value(QStringLiteral("payload")).toObject().value(QStringLiteral("pin")).toString(),
           QStringLiteral("123456"));
}

void TestProtocol::parsesResponseFrame() {
  const QByteArray raw =
      QJsonDocument(QJsonObject{
                        {QStringLiteral("id"), QStringLiteral("42")},
                        {QStringLiteral("type"), QStringLiteral("response")},
                        {QStringLiteral("success"), true},
                        {QStringLiteral("payload"), QJsonObject{{QStringLiteral("quiz_code"), QStringLiteral("Q0")}}},
                    })
          .toJson(QJsonDocument::Compact);
  const auto frame = parseServerFrame(raw);
  QCOMPARE(frame.kind, ServerFrame::Kind::Response);
  QCOMPARE(frame.id, QStringLiteral("42"));
  QVERIFY(frame.success);
  QCOMPARE(frame.payload.value(QStringLiteral("quiz_code")).toString(), QStringLiteral("Q0"));
}

void TestProtocol::parsesQuestionStartedEvent() {
  const QByteArray raw =
      QJsonDocument(QJsonObject{{QStringLiteral("type"), QStringLiteral("event")},
                                {QStringLiteral("event_type"), QStringLiteral("question_started")},
                                {QStringLiteral("payload"),
                                 QJsonObject{
                                     {QStringLiteral("question_index"), 2},
                                     {QStringLiteral("total_questions"), 7},
                                     {QStringLiteral("duration_ms"), 8000},
                                     {QStringLiteral("text"), QStringLiteral("What?")},
                                     {QStringLiteral("answer_type"), QStringLiteral("single_choice")},
                                     {QStringLiteral("options"), QJsonArray{QStringLiteral("A"), QStringLiteral("B")}},
                                 }}})
          .toJson(QJsonDocument::Compact);
  const auto frame = parseServerFrame(raw);
  QCOMPARE(frame.kind, ServerFrame::Kind::Event);
  QVERIFY(frame.hasEvent);
  const auto* ev = std::get_if<QuestionStartedEv>(&frame.event);
  QVERIFY(ev != nullptr);
  QCOMPARE(ev->questionIndex, 2);
  QCOMPARE(ev->totalQuestions, 7);
  QCOMPARE(ev->durationMs, 8000);
  QCOMPARE(ev->question.text, QStringLiteral("What?"));
  QCOMPARE(ev->question.options.size(), 2);
  QCOMPARE(ev->question.type, AnswerType::SingleChoice);
}

void TestProtocol::parsesTimerUpdateEvent() {
  const QByteArray raw =
      QJsonDocument(QJsonObject{{QStringLiteral("type"), QStringLiteral("event")},
                                {QStringLiteral("event_type"), QStringLiteral("timer_update")},
                                {QStringLiteral("payload"), QJsonObject{{QStringLiteral("remaining_ms"), 1234}}}})
          .toJson(QJsonDocument::Compact);
  const auto frame = parseServerFrame(raw);
  const auto* ev = std::get_if<TimerUpdateEv>(&frame.event);
  QVERIFY(ev != nullptr);
  QCOMPARE(ev->remainingMs, 1234);
}

void TestProtocol::parsesAnswerRevealEvent() {
  const QByteArray raw =
      QJsonDocument(QJsonObject{{QStringLiteral("type"), QStringLiteral("event")},
                                {QStringLiteral("event_type"), QStringLiteral("answer_reveal")},
                                {QStringLiteral("payload"),
                                 QJsonObject{{QStringLiteral("correct_indices"), QJsonArray{1}},
                                             {QStringLiteral("my_selected_indices"), QJsonArray{0}},
                                             {QStringLiteral("option_pick_counts"), QJsonArray{2, 1, 0}},
                                             {QStringLiteral("reveal_duration_ms"), 5000}}}})
          .toJson(QJsonDocument::Compact);
  const auto frame = parseServerFrame(raw);
  const auto* ev = std::get_if<AnswerRevealEv>(&frame.event);
  QVERIFY(ev != nullptr);
  QCOMPARE(ev->correctIndices, QVector<int>({1}));
  QCOMPARE(ev->mySelectedIndices, QVector<int>({0}));
  QCOMPARE(ev->optionPickCounts, QVector<int>({2, 1, 0}));
  QCOMPARE(ev->revealDurationMs, 5000);
}

void TestProtocol::parsesLeaderboardEvent() {
  const QByteArray raw =
      QJsonDocument(
          QJsonObject{{QStringLiteral("type"), QStringLiteral("event")},
                      {QStringLiteral("event_type"), QStringLiteral("leaderboard")},
                      {QStringLiteral("payload"),
                       QJsonObject{{QStringLiteral("entries"),
                                    QJsonArray{QJsonObject{{QStringLiteral("player_id"), QStringLiteral("P0")},
                                                           {QStringLiteral("score"), 200}},
                                               QJsonObject{{QStringLiteral("player_id"), QStringLiteral("P1")},
                                                           {QStringLiteral("score"), 100}}}},
                                   {QStringLiteral("next_round_delay_ms"), 1500}}}})
          .toJson(QJsonDocument::Compact);
  const auto frame = parseServerFrame(raw);
  const auto* ev = std::get_if<LeaderboardEv>(&frame.event);
  QVERIFY(ev != nullptr);
  QCOMPARE(ev->entries.size(), 2);
  QCOMPARE(ev->entries.first().playerId, QStringLiteral("P0"));
  QCOMPARE(ev->entries.first().score, 200);
  QCOMPARE(ev->nextRoundDelayMs, 1500);
}

void TestProtocol::parsesPlayerJoinedEvent() {
  const QByteArray raw =
      QJsonDocument(QJsonObject{{QStringLiteral("type"), QStringLiteral("event")},
                                {QStringLiteral("event_type"), QStringLiteral("player_joined")},
                                {QStringLiteral("payload"),
                                 QJsonObject{{QStringLiteral("player_id"), QStringLiteral("P7")},
                                             {QStringLiteral("display_name"), QStringLiteral("Alice")},
                                             {QStringLiteral("role"), QStringLiteral("player")},
                                             {QStringLiteral("is_competing"), false}}}})
          .toJson(QJsonDocument::Compact);
  const auto frame = parseServerFrame(raw);
  const auto* ev = std::get_if<PlayerJoinedEv>(&frame.event);
  QVERIFY(ev != nullptr);
  QCOMPARE(ev->playerId, QStringLiteral("P7"));
  QCOMPARE(ev->displayName, QStringLiteral("Alice"));
  QCOMPARE(ev->role, QStringLiteral("player"));
  QCOMPARE(ev->isCompeting, false);
}

void TestProtocol::parsesGameFinishedEvent() {
  const QByteArray raw =
      QJsonDocument(
          QJsonObject{{QStringLiteral("type"), QStringLiteral("event")},
                      {QStringLiteral("event_type"), QStringLiteral("game_finished")},
                      {QStringLiteral("payload"),
                       QJsonObject{{QStringLiteral("final_leaderboard"),
                                    QJsonArray{QJsonObject{{QStringLiteral("player_id"), QStringLiteral("P0")},
                                                           {QStringLiteral("score"), 300}}}}}}})
          .toJson(QJsonDocument::Compact);
  const auto frame = parseServerFrame(raw);
  const auto* ev = std::get_if<GameFinishedEv>(&frame.event);
  QVERIFY(ev != nullptr);
  QCOMPARE(ev->finalLeaderboard.size(), 1);
  QCOMPARE(ev->finalLeaderboard.first().score, 300);
}

QTEST_MAIN(TestProtocol)
#include "test_protocol.moc"
