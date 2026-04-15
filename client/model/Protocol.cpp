#include "model/Protocol.hpp"

#include <QJsonDocument>

namespace quizlyx::client::model {

namespace {

QVector<LeaderboardRow> parseLeaderboard(const QJsonArray& array) {
  QVector<LeaderboardRow> rows;
  rows.reserve(array.size());
  for (const auto& v : array) {
    const auto obj = v.toObject();
    LeaderboardRow row;
    row.playerId = obj.value(QStringLiteral("player_id")).toString();
    row.score = obj.value(QStringLiteral("score")).toInt();
    rows.push_back(row);
  }
  return rows;
}

QVector<int> parseIntArray(const QJsonArray& array) {
  QVector<int> values;
  values.reserve(array.size());
  for (const auto& value : array)
    values.push_back(value.toInt());
  return values;
}

} // namespace

QJsonObject toJson(const Question& question) {
  QJsonArray options;
  for (const auto& opt : question.options)
    options.push_back(opt);
  QJsonArray correct;
  for (int idx : question.correctIndices)
    correct.push_back(idx);
  return QJsonObject{
      {QStringLiteral("text"), question.text},
      {QStringLiteral("answer_type"), answerTypeToString(question.type)},
      {QStringLiteral("options"), options},
      {QStringLiteral("correct_indices"), correct},
      {QStringLiteral("time_limit_ms"), question.timeLimitMs},
  };
}

QJsonObject toJson(const Quiz& quiz) {
  QJsonArray questions;
  for (const auto& q : quiz.questions)
    questions.push_back(toJson(q));
  return QJsonObject{
      {QStringLiteral("title"), quiz.title},
      {QStringLiteral("description"), quiz.description},
      {QStringLiteral("questions"), questions},
  };
}

Question questionFromJson(const QJsonObject& obj) {
  Question q;
  q.text = obj.value(QStringLiteral("text")).toString();
  q.type = answerTypeFromString(obj.value(QStringLiteral("answer_type")).toString());
  const auto opts = obj.value(QStringLiteral("options")).toArray();
  q.options.clear();
  for (const auto& v : opts)
    q.options.push_back(v.toString());
  const auto correct = obj.value(QStringLiteral("correct_indices")).toArray();
  q.correctIndices.clear();
  for (const auto& v : correct)
    q.correctIndices.push_back(v.toInt());
  q.timeLimitMs = obj.value(QStringLiteral("time_limit_ms")).toInt();
  return q;
}

Quiz quizFromJson(const QJsonObject& obj) {
  Quiz quiz;
  quiz.title = obj.value(QStringLiteral("title")).toString();
  quiz.description = obj.value(QStringLiteral("description")).toString();
  const auto qs = obj.value(QStringLiteral("questions")).toArray();
  quiz.questions.clear();
  for (const auto& v : qs)
    quiz.questions.push_back(questionFromJson(v.toObject()));
  return quiz;
}

QJsonObject makeCommand(const QString& id, const QString& type, const QJsonObject& payload) {
  return QJsonObject{
      {QStringLiteral("id"), id},
      {QStringLiteral("type"), type},
      {QStringLiteral("payload"), payload},
  };
}

ServerFrame parseServerFrame(const QByteArray& raw) {
  ServerFrame frame;
  QJsonParseError err{};
  const auto doc = QJsonDocument::fromJson(raw, &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject())
    return frame;

  const auto obj = doc.object();
  const auto type = obj.value(QStringLiteral("type")).toString();
  const auto payload = obj.value(QStringLiteral("payload")).toObject();

  if (type == QStringLiteral("response")) {
    frame.kind = ServerFrame::Kind::Response;
    frame.id = obj.value(QStringLiteral("id")).toString();
    frame.success = obj.value(QStringLiteral("success")).toBool();
    frame.payload = payload;
    return frame;
  }

  if (type == QStringLiteral("event")) {
    frame.kind = ServerFrame::Kind::Event;
    const auto eventType = obj.value(QStringLiteral("event_type")).toString();

    if (eventType == QStringLiteral("question_started")) {
      QuestionStartedEv ev;
      ev.questionIndex = payload.value(QStringLiteral("question_index")).toInt();
      ev.totalQuestions = payload.value(QStringLiteral("total_questions")).toInt();
      ev.durationMs = payload.value(QStringLiteral("duration_ms")).toInt();
      ev.question.text = payload.value(QStringLiteral("text")).toString();
      ev.question.type = answerTypeFromString(payload.value(QStringLiteral("answer_type")).toString());
      const auto opts = payload.value(QStringLiteral("options")).toArray();
      ev.question.options.clear();
      for (const auto& v : opts)
        ev.question.options.push_back(v.toString());
      ev.question.timeLimitMs = ev.durationMs;
      frame.event = ev;
      frame.hasEvent = true;
    } else if (eventType == QStringLiteral("timer_update")) {
      TimerUpdateEv ev;
      ev.remainingMs = payload.value(QStringLiteral("remaining_ms")).toInt();
      frame.event = ev;
      frame.hasEvent = true;
    } else if (eventType == QStringLiteral("question_timeout")) {
      frame.event = QuestionTimeoutEv{};
      frame.hasEvent = true;
    } else if (eventType == QStringLiteral("answer_reveal")) {
      AnswerRevealEv ev;
      ev.correctIndices = parseIntArray(payload.value(QStringLiteral("correct_indices")).toArray());
      ev.mySelectedIndices = parseIntArray(payload.value(QStringLiteral("my_selected_indices")).toArray());
      ev.optionPickCounts = parseIntArray(payload.value(QStringLiteral("option_pick_counts")).toArray());
      ev.revealDurationMs = payload.value(QStringLiteral("reveal_duration_ms")).toInt();
      frame.event = ev;
      frame.hasEvent = true;
    } else if (eventType == QStringLiteral("leaderboard")) {
      LeaderboardEv ev;
      ev.entries = parseLeaderboard(payload.value(QStringLiteral("entries")).toArray());
      ev.nextRoundDelayMs = payload.value(QStringLiteral("next_round_delay_ms")).toInt();
      frame.event = ev;
      frame.hasEvent = true;
    } else if (eventType == QStringLiteral("player_joined")) {
      PlayerJoinedEv ev;
      ev.playerId = payload.value(QStringLiteral("player_id")).toString();
      ev.displayName = payload.value(QStringLiteral("display_name")).toString();
      ev.role = payload.value(QStringLiteral("role")).toString();
      ev.isCompeting = payload.value(QStringLiteral("is_competing")).toBool(true);
      frame.event = ev;
      frame.hasEvent = true;
    } else if (eventType == QStringLiteral("player_left")) {
      PlayerLeftEv ev;
      ev.playerId = payload.value(QStringLiteral("player_id")).toString();
      frame.event = ev;
      frame.hasEvent = true;
    } else if (eventType == QStringLiteral("game_finished")) {
      GameFinishedEv ev;
      ev.finalLeaderboard = parseLeaderboard(payload.value(QStringLiteral("final_leaderboard")).toArray());
      frame.event = ev;
      frame.hasEvent = true;
    }
    return frame;
  }

  return frame;
}

} // namespace quizlyx::client::model
