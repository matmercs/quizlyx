#ifndef QUIZLYX_CLIENT_MODEL_QUIZ_HPP
#define QUIZLYX_CLIENT_MODEL_QUIZ_HPP

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

namespace quizlyx::client::model {

enum class AnswerType { SingleChoice, MultipleChoice };

struct Question {
  QString text;
  AnswerType type = AnswerType::SingleChoice;
  QStringList options;
  QVector<int> correctIndices;
  int timeLimitMs = 30000;
};

struct Quiz {
  QString title;
  QString description;
  QVector<Question> questions;
};

bool isValid(const Question& question);
bool isValid(const Quiz& quiz);

QString answerTypeToString(AnswerType type);
AnswerType answerTypeFromString(const QString& s);

} // namespace quizlyx::client::model

Q_DECLARE_METATYPE(quizlyx::client::model::AnswerType)
Q_DECLARE_METATYPE(quizlyx::client::model::Question)
Q_DECLARE_METATYPE(quizlyx::client::model::Quiz)

#endif // QUIZLYX_CLIENT_MODEL_QUIZ_HPP
