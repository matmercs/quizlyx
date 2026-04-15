#include "ui/pages/PlayerLobbyPage.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "model/SessionState.hpp"
#include "ui/widgets/PlayerListWidget.hpp"

namespace quizlyx::client::ui::pages {

PlayerLobbyPage::PlayerLobbyPage(model::SessionState* state, QWidget* parent) : QWidget(parent), state_(state) {
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(40, 40, 40, 40);
  root->setSpacing(16);

  auto* title = new QLabel(QStringLiteral("Waiting for the host…"));
  title->setObjectName(QStringLiteral("titleLabel"));
  title->setAlignment(Qt::AlignCenter);
  root->addWidget(title);

  greetingLabel_ = new QLabel;
  greetingLabel_->setObjectName(QStringLiteral("subtitleLabel"));
  greetingLabel_->setAlignment(Qt::AlignCenter);
  root->addWidget(greetingLabel_);

  countLabel_ = new QLabel;
  countLabel_->setObjectName(QStringLiteral("subtitleLabel"));
  countLabel_->setAlignment(Qt::AlignCenter);
  root->addWidget(countLabel_);

  playerList_ = new widgets::PlayerListWidget;
  root->addWidget(playerList_, 1);

  auto* btnRow = new QHBoxLayout;
  auto* leaveBtn = new QPushButton(QStringLiteral("Leave"));
  btnRow->addWidget(leaveBtn);
  btnRow->addStretch(1);
  root->addLayout(btnRow);

  connect(leaveBtn, &QPushButton::clicked, this, &PlayerLobbyPage::leaveRequested);
  connect(state_, &model::SessionState::playersChanged, this, &PlayerLobbyPage::refresh);
  connect(state_, &model::SessionState::myIdentityChanged, this, &PlayerLobbyPage::refresh);
  refresh();
}

void PlayerLobbyPage::refresh() {
  int competingConnected = 0;
  for (const auto& player : state_->players()) {
    if (player.isCompeting && player.connected)
      ++competingConnected;
  }

  if (!state_->myDisplayName().isEmpty())
    greetingLabel_->setText(QStringLiteral("Hi, %1!").arg(state_->myDisplayName()));
  else
    greetingLabel_->clear();
  countLabel_->setText(QStringLiteral("%1 players in lobby").arg(competingConnected));
  playerList_->setPlayers(state_->players(), state_->myPlayerId());
}

} // namespace quizlyx::client::ui::pages
