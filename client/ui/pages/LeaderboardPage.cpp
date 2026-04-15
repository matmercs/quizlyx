#include "ui/pages/LeaderboardPage.hpp"

#include <QDateTime>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "model/SessionState.hpp"
#include "ui/widgets/LeaderboardWidget.hpp"

namespace quizlyx::client::ui::pages {

LeaderboardPage::LeaderboardPage(model::SessionState* state, QWidget* parent) : QWidget(parent), state_(state) {
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(40, 32, 40, 32);
  root->setSpacing(16);

  titleLabel_ = new QLabel(QStringLiteral("Leaderboard"));
  titleLabel_->setObjectName(QStringLiteral("titleLabel"));
  titleLabel_->setAlignment(Qt::AlignCenter);
  root->addWidget(titleLabel_);

  countdownLabel_ = new QLabel;
  countdownLabel_->setObjectName(QStringLiteral("subtitleLabel"));
  countdownLabel_->setAlignment(Qt::AlignCenter);
  countdownLabel_->hide();
  root->addWidget(countdownLabel_);

  board_ = new widgets::LeaderboardWidget;
  root->addWidget(board_, 1);

  auto* btnRow = new QHBoxLayout;
  auto* leaveBtn = new QPushButton(QStringLiteral("Leave"));
  nextBtn_ = new QPushButton(QStringLiteral("Next question"));
  nextBtn_->setObjectName(QStringLiteral("primaryButton"));
  btnRow->addWidget(leaveBtn);
  btnRow->addStretch(1);
  btnRow->addWidget(nextBtn_);
  root->addLayout(btnRow);

  connect(nextBtn_, &QPushButton::clicked, this, &LeaderboardPage::nextQuestionRequested);
  connect(leaveBtn, &QPushButton::clicked, this, &LeaderboardPage::leaveRequested);
  connect(state_, &model::SessionState::leaderboardChanged, this, &LeaderboardPage::refresh);
  connect(state_, &model::SessionState::phaseChanged, this, &LeaderboardPage::refresh);
  connect(state_, &model::SessionState::playersChanged, this, &LeaderboardPage::refresh);
  connect(&countdownTimer_, &QTimer::timeout, this, &LeaderboardPage::updateCountdown);
  countdownTimer_.setInterval(100);
  refresh();
}

void LeaderboardPage::refresh() {
  QHash<QString, QString> nameById;
  for (const auto& p : state_->players())
    nameById.insert(p.id, p.name.isEmpty() ? p.id : p.name);
  board_->setRows(state_->leaderboard(), nameById, state_->myPlayerId());
  nextBtn_->setVisible(state_->role() == model::Role::Host);

  if (state_->phase() == model::Phase::Leaderboard && state_->nextRoundDelayMs() > 0) {
    countdownDeadlineMs_ = QDateTime::currentMSecsSinceEpoch() + state_->nextRoundDelayMs();
    countdownTimer_.start();
    updateCountdown();
  } else {
    countdownTimer_.stop();
    countdownLabel_->clear();
    countdownLabel_->hide();
  }
}

void LeaderboardPage::updateCountdown() {
  const auto remainingMs = static_cast<int>(countdownDeadlineMs_ - QDateTime::currentMSecsSinceEpoch());
  if (remainingMs <= 0) {
    countdownTimer_.stop();
    countdownLabel_->setText(QStringLiteral("Starting next question…"));
    countdownLabel_->show();
    return;
  }

  countdownLabel_->setText(
      QStringLiteral("Next question starts automatically in %1 s").arg(QString::number(remainingMs / 1000.0, 'f', 1)));
  countdownLabel_->show();
}

} // namespace quizlyx::client::ui::pages
