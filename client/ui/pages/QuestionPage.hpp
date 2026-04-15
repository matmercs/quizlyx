#ifndef QUIZLYX_CLIENT_UI_PAGES_QUESTION_PAGE_HPP
#define QUIZLYX_CLIENT_UI_PAGES_QUESTION_PAGE_HPP

#include <QVector>
#include <QWidget>

class QGridLayout;
class QLabel;
class QPushButton;

namespace quizlyx::client::model {
class SessionState;
} // namespace quizlyx::client::model

namespace quizlyx::client::ui::widgets {
class AnswerButton;
class CountdownBar;
} // namespace quizlyx::client::ui::widgets

namespace quizlyx::client::ui::pages {

class QuestionPage : public QWidget {
  Q_OBJECT
public:
  explicit QuestionPage(model::SessionState* state, QWidget* parent = nullptr);

signals:
  void submitAnswer(QVector<int> selectedIndices);

private slots:
  void onQuestionStarted(int questionIndex, int durationMs);
  void onRemainingMsChanged(int remainingMs);
  void onQuestionTimedOut();
  void onAnswerRevealStarted();
  void onButtonClicked(int index);
  void onConfirmClicked();

private:
  void rebuildAnswerButtons();
  void lockInputs();
  QString optionSummary(const QVector<int>& indices, const QString& emptyLabel) const;

  model::SessionState* state_;
  QLabel* indexLabel_ = nullptr;
  QLabel* textLabel_ = nullptr;
  QLabel* hintLabel_ = nullptr;
  QLabel* statusLabel_ = nullptr;
  widgets::CountdownBar* countdown_ = nullptr;
  QPushButton* confirmBtn_ = nullptr;
  QGridLayout* answerGrid_ = nullptr;

  QVector<widgets::AnswerButton*> answerButtons_;
  QVector<int> selection_;
  bool submitted_ = false;
};

} // namespace quizlyx::client::ui::pages

#endif // QUIZLYX_CLIENT_UI_PAGES_QUESTION_PAGE_HPP
