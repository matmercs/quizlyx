#include "ui/pages/FinalPage.hpp"

#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "model/SessionState.hpp"
#include "ui/widgets/LeaderboardWidget.hpp"

namespace quizlyx::client::ui::pages {

FinalPage::FinalPage(model::SessionState* state, QWidget* parent) : QWidget(parent), state_(state) {
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(40, 40, 40, 40);
  root->setSpacing(18);

  auto* title = new QLabel(QStringLiteral("Game over!"));
  title->setObjectName(QStringLiteral("titleLabel"));
  title->setAlignment(Qt::AlignCenter);
  root->addWidget(title);

  winnerLabel_ = new QLabel;
  winnerLabel_->setObjectName(QStringLiteral("subtitleLabel"));
  winnerLabel_->setAlignment(Qt::AlignCenter);
  root->addWidget(winnerLabel_);

  board_ = new widgets::LeaderboardWidget;
  root->addWidget(board_, 1);

  auto* btnRow = new QHBoxLayout;
  auto* backBtn = new QPushButton(QStringLiteral("Back to menu"));
  backBtn->setObjectName(QStringLiteral("primaryButton"));
  btnRow->addStretch(1);
  btnRow->addWidget(backBtn);
  btnRow->addStretch(1);
  root->addLayout(btnRow);

  connect(backBtn, &QPushButton::clicked, this, &FinalPage::backToMenu);
  connect(state_, &model::SessionState::leaderboardChanged, this, &FinalPage::refresh);
  connect(state_, &model::SessionState::gameFinished, this, &FinalPage::refresh);
  refresh();
}

void FinalPage::refresh() {
  QHash<QString, QString> nameById;
  for (const auto& p : state_->players())
    nameById.insert(p.id, p.name.isEmpty() ? p.id : p.name);
  board_->setRows(state_->leaderboard(), nameById, state_->myPlayerId());
  if (!state_->leaderboard().isEmpty()) {
    const auto& top = state_->leaderboard().first();
    const auto name = nameById.value(top.playerId, top.playerId);
    winnerLabel_->setText(QStringLiteral("🏆  %1 wins with %2 points").arg(name).arg(top.score));
  } else {
    winnerLabel_->clear();
  }
}

} // namespace quizlyx::client::ui::pages
