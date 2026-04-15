#ifndef QUIZLYX_CLIENT_MODEL_PROTOCOL_HPP
#define QUIZLYX_CLIENT_MODEL_PROTOCOL_HPP

#include <QJsonArray>
#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QVector>
#include <variant>

#include "model/Quiz.hpp"
#include "model/SessionState.hpp"

namespace quizlyx::client::model {

struct QuestionStartedEv {
  int questionIndex = 0;
  int totalQuestions = 0;
  int durationMs = 0;
  Question question;
};

struct TimerUpdateEv {
  int remainingMs = 0;
};

struct QuestionTimeoutEv {};

struct AnswerRevealEv {
  QVector<int> correctIndices;
  QVector<int> mySelectedIndices;
  QVector<int> optionPickCounts;
  int revealDurationMs = 0;
};

struct LeaderboardEv {
  QVector<LeaderboardRow> entries;
  int nextRoundDelayMs = 0;
};

struct PlayerJoinedEv {
  QString playerId;
  QString displayName;
  QString role;
  bool isCompeting = true;
};

struct PlayerLeftEv {
  QString playerId;
};

struct GameFinishedEv {
  QVector<LeaderboardRow> finalLeaderboard;
};

using ServerEvent = std::variant<QuestionStartedEv,
                                 TimerUpdateEv,
                                 QuestionTimeoutEv,
                                 AnswerRevealEv,
                                 LeaderboardEv,
                                 PlayerJoinedEv,
                                 PlayerLeftEv,
                                 GameFinishedEv>;

struct ServerFrame {
  enum class Kind { Response, Event, Invalid };
  Kind kind = Kind::Invalid;
  QString id;
  bool success = false;
  QJsonObject payload;
  ServerEvent event;
  bool hasEvent = false;
};

QJsonObject toJson(const Question& question);
QJsonObject toJson(const Quiz& quiz);
Question questionFromJson(const QJsonObject& obj);
Quiz quizFromJson(const QJsonObject& obj);

QJsonObject makeCommand(const QString& id, const QString& type, const QJsonObject& payload);

ServerFrame parseServerFrame(const QByteArray& raw);

} // namespace quizlyx::client::model

Q_DECLARE_METATYPE(quizlyx::client::model::QuestionStartedEv)
Q_DECLARE_METATYPE(quizlyx::client::model::TimerUpdateEv)
Q_DECLARE_METATYPE(quizlyx::client::model::QuestionTimeoutEv)
Q_DECLARE_METATYPE(quizlyx::client::model::AnswerRevealEv)
Q_DECLARE_METATYPE(quizlyx::client::model::LeaderboardEv)
Q_DECLARE_METATYPE(quizlyx::client::model::PlayerJoinedEv)
Q_DECLARE_METATYPE(quizlyx::client::model::PlayerLeftEv)
Q_DECLARE_METATYPE(quizlyx::client::model::GameFinishedEv)

#endif // QUIZLYX_CLIENT_MODEL_PROTOCOL_HPP
