#include "ui/pages/QuestionPage.hpp"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>
#include <algorithm>

#include "model/SessionState.hpp"
#include "ui/widgets/AnswerButton.hpp"
#include "ui/widgets/CountdownBar.hpp"

namespace quizlyx::client::ui::pages {

QuestionPage::QuestionPage(model::SessionState* state, QWidget* parent) : QWidget(parent), state_(state) {
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(32, 24, 32, 24);
  root->setSpacing(14);

  auto* topRow = new QHBoxLayout;
  indexLabel_ = new QLabel(QStringLiteral("0 / 0"));
  indexLabel_->setObjectName(QStringLiteral("subtitleLabel"));
  countdown_ = new widgets::CountdownBar;
  topRow->addWidget(indexLabel_);
  topRow->addWidget(countdown_, 1);
  root->addLayout(topRow);

  textLabel_ = new QLabel;
  textLabel_->setObjectName(QStringLiteral("questionText"));
  textLabel_->setWordWrap(true);
  textLabel_->setAlignment(Qt::AlignCenter);
  root->addWidget(textLabel_);

  hintLabel_ = new QLabel;
  hintLabel_->setObjectName(QStringLiteral("subtitleLabel"));
  hintLabel_->setAlignment(Qt::AlignCenter);
  root->addWidget(hintLabel_);

  auto* gridHolder = new QWidget;
  answerGrid_ = new QGridLayout(gridHolder);
  answerGrid_->setSpacing(12);
  root->addWidget(gridHolder, 1);

  auto* bottomRow = new QHBoxLayout;
  statusLabel_ = new QLabel;
  statusLabel_->setObjectName(QStringLiteral("statusLabel"));
  statusLabel_->setWordWrap(true);
  confirmBtn_ = new QPushButton(QStringLiteral("Confirm"));
  confirmBtn_->setObjectName(QStringLiteral("primaryButton"));
  confirmBtn_->setVisible(false);
  bottomRow->addWidget(statusLabel_, 1);
  bottomRow->addWidget(confirmBtn_);
  root->addLayout(bottomRow);

  connect(confirmBtn_, &QPushButton::clicked, this, &QuestionPage::onConfirmClicked);
  connect(state_, &model::SessionState::questionStarted, this, &QuestionPage::onQuestionStarted);
  connect(state_, &model::SessionState::remainingMsChanged, this, &QuestionPage::onRemainingMsChanged);
  connect(state_, &model::SessionState::questionTimedOut, this, &QuestionPage::onQuestionTimedOut);
  connect(state_, &model::SessionState::answerRevealStarted, this, &QuestionPage::onAnswerRevealStarted);
}

void QuestionPage::rebuildAnswerButtons() {
  for (auto* btn : answerButtons_) {
    answerGrid_->removeWidget(btn);
    btn->deleteLater();
  }
  answerButtons_.clear();

  if (answerGrid_ == nullptr || !state_->hasCurrentQuestion())
    return;

  const auto& q = state_->currentQuestion();
  for (int i = 0; i < q.options.size(); ++i) {
    auto* btn = new widgets::AnswerButton(i, q.options.at(i), this);
    connect(btn, &widgets::AnswerButton::answerClicked, this, &QuestionPage::onButtonClicked);
    const int row = i / 2;
    const int col = i % 2;
    answerGrid_->addWidget(btn, row, col);
    answerButtons_.push_back(btn);
  }
}

void QuestionPage::onQuestionStarted(int questionIndex, int durationMs) {
  submitted_ = false;
  selection_.clear();
  if (!state_->hasCurrentQuestion())
    return;
  const auto& q = state_->currentQuestion();
  const bool canAnswer = state_->isCompeting();
  indexLabel_->setText(QStringLiteral("Question %1 / %2").arg(questionIndex + 1).arg(state_->totalQuestions()));
  textLabel_->setText(q.text);
  if (!canAnswer) {
    hintLabel_->setText(QStringLiteral("Spectating this round"));
  } else {
    hintLabel_->setText(q.type == model::AnswerType::MultipleChoice
                            ? QStringLiteral("Pick all that apply, then press Confirm")
                            : QStringLiteral("Pick the correct answer"));
  }
  confirmBtn_->setVisible(q.type == model::AnswerType::MultipleChoice && canAnswer);
  confirmBtn_->setEnabled(false);
  statusLabel_->setText(canAnswer ? QString()
                                  : QStringLiteral("Spectating. Answers and counts will appear after time ends."));
  rebuildAnswerButtons();
  countdown_->setDuration(durationMs);
  if (!canAnswer)
    lockInputs();
}

void QuestionPage::onRemainingMsChanged(int remainingMs) {
  countdown_->setRemainingMs(remainingMs);
}

void QuestionPage::onQuestionTimedOut() {
  countdown_->stop();
  statusLabel_->setText(QStringLiteral("Time's up. Waiting for reveal…"));
  lockInputs();
}

void QuestionPage::onAnswerRevealStarted() {
  if (!state_->hasCurrentQuestion() || !state_->isRevealActive())
    return;

  countdown_->stop();
  lockInputs();
  hintLabel_->setText(QStringLiteral("Answer reveal"));

  const auto& reveal = state_->revealState();
  for (auto* btn : answerButtons_) {
    const int optionIndex = btn->index();
    const bool selected = reveal.mySelectedIndices.contains(optionIndex);
    const bool correct = reveal.correctIndices.contains(optionIndex);
    btn->setChecked(selected);
    btn->showPickCount(true);
    btn->setPickCount(optionIndex < reveal.optionPickCounts.size() ? reveal.optionPickCounts.at(optionIndex) : 0);
    if (correct) {
      btn->setRevealState(widgets::AnswerButton::RevealState::Correct);
    } else if (selected) {
      btn->setRevealState(widgets::AnswerButton::RevealState::WrongSelected);
    } else {
      btn->setRevealState(widgets::AnswerButton::RevealState::None);
    }
  }

  const auto correctText = optionSummary(reveal.correctIndices, QStringLiteral("No correct answer"));
  if (!state_->isCompeting()) {
    statusLabel_->setText(QStringLiteral("Spectating. Correct answer: %1").arg(correctText));
  } else if (reveal.mySelectedIndices.isEmpty()) {
    statusLabel_->setText(QStringLiteral("No answer. Correct answer: %1").arg(correctText));
  } else {
    statusLabel_->setText(QStringLiteral("You answered: %1. Correct answer: %2")
                              .arg(optionSummary(reveal.mySelectedIndices, QStringLiteral("No answer")), correctText));
  }
}

void QuestionPage::onButtonClicked(int index) {
  if (submitted_ || !state_->hasCurrentQuestion() || !state_->isCompeting())
    return;
  const auto& q = state_->currentQuestion();

  if (q.type == model::AnswerType::SingleChoice) {
    selection_ = {index};
    for (auto* btn : answerButtons_)
      btn->setChecked(btn->index() == index);
    lockInputs();
    submitted_ = true;
    statusLabel_->setText(QStringLiteral("Answer locked"));
    emit submitAnswer(selection_);
    return;
  }

  if (const auto pos = selection_.indexOf(index); pos >= 0) {
    selection_.remove(pos);
  } else {
    selection_.push_back(index);
  }
  std::sort(selection_.begin(), selection_.end());
  for (auto* btn : answerButtons_)
    btn->setChecked(selection_.contains(btn->index()));
  confirmBtn_->setEnabled(!selection_.isEmpty());
}

void QuestionPage::onConfirmClicked() {
  if (submitted_ || selection_.isEmpty())
    return;
  submitted_ = true;
  lockInputs();
  statusLabel_->setText(QStringLiteral("Answer locked"));
  emit submitAnswer(selection_);
}

void QuestionPage::lockInputs() {
  for (auto* btn : answerButtons_)
    btn->setLocked(true);
  confirmBtn_->setEnabled(false);
}

QString QuestionPage::optionSummary(const QVector<int>& indices, const QString& emptyLabel) const {
  if (!state_->hasCurrentQuestion() || indices.isEmpty())
    return emptyLabel;

  const auto& question = state_->currentQuestion();
  QStringList labels;
  for (const int index : indices) {
    if (index >= 0 && index < question.options.size())
      labels << question.options.at(index);
  }
  return labels.isEmpty() ? emptyLabel : labels.join(QStringLiteral(", "));
}

} // namespace quizlyx::client::ui::pages
