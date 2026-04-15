#include "model/Quiz.hpp"

namespace quizlyx::client::model {

bool isValid(const Question& question) {
  if (question.text.trimmed().isEmpty())
    return false;
  if (question.options.size() < 2)
    return false;
  if (question.correctIndices.isEmpty())
    return false;
  if (question.timeLimitMs <= 0)
    return false;
  for (int idx : question.correctIndices) {
    if (idx < 0 || idx >= question.options.size())
      return false;
  }
  if (question.type == AnswerType::SingleChoice && question.correctIndices.size() != 1)
    return false;
  return true;
}

bool isValid(const Quiz& quiz) {
  if (quiz.title.trimmed().isEmpty())
    return false;
  if (quiz.questions.isEmpty())
    return false;
  for (const auto& question : quiz.questions) {
    if (!isValid(question))
      return false;
  }
  return true;
}

QString answerTypeToString(AnswerType type) {
  return type == AnswerType::MultipleChoice ? QStringLiteral("multiple_choice") : QStringLiteral("single_choice");
}

AnswerType answerTypeFromString(const QString& s) {
  return s == QStringLiteral("multiple_choice") ? AnswerType::MultipleChoice : AnswerType::SingleChoice;
}

} // namespace quizlyx::client::model
