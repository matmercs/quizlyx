#include "ui/pages/HostLobbyPage.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "model/SessionState.hpp"
#include "ui/widgets/PlayerListWidget.hpp"

namespace quizlyx::client::ui::pages {

HostLobbyPage::HostLobbyPage(model::SessionState* state, QWidget* parent) : QWidget(parent), state_(state) {
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(40, 40, 40, 40);
  root->setSpacing(16);

  auto* title = new QLabel(QStringLiteral("Share this PIN"));
  title->setObjectName(QStringLiteral("titleLabel"));
  title->setAlignment(Qt::AlignCenter);
  root->addWidget(title);

  pinLabel_ = new QLabel(QStringLiteral("------"));
  pinLabel_->setObjectName(QStringLiteral("pinLabel"));
  pinLabel_->setAlignment(Qt::AlignCenter);
  root->addWidget(pinLabel_);

  playerCountLabel_ = new QLabel;
  playerCountLabel_->setObjectName(QStringLiteral("subtitleLabel"));
  playerCountLabel_->setAlignment(Qt::AlignCenter);
  root->addWidget(playerCountLabel_);

  playerList_ = new widgets::PlayerListWidget;
  root->addWidget(playerList_, 1);

  auto* btnRow = new QHBoxLayout;
  auto* leaveBtn = new QPushButton(QStringLiteral("Cancel"));
  startBtn_ = new QPushButton(QStringLiteral("Start game"));
  startBtn_->setObjectName(QStringLiteral("primaryButton"));
  startBtn_->setEnabled(false);
  btnRow->addWidget(leaveBtn);
  btnRow->addStretch(1);
  btnRow->addWidget(startBtn_);
  root->addLayout(btnRow);

  connect(startBtn_, &QPushButton::clicked, this, &HostLobbyPage::startRequested);
  connect(leaveBtn, &QPushButton::clicked, this, &HostLobbyPage::leaveRequested);

  connect(state_, &model::SessionState::playersChanged, this, &HostLobbyPage::refresh);
  connect(state_, &model::SessionState::myIdentityChanged, this, &HostLobbyPage::refresh);
  refresh();
}

void HostLobbyPage::refresh() {
  int competingConnected = 0;
  for (const auto& player : state_->players()) {
    if (player.isCompeting && player.connected)
      ++competingConnected;
  }

  pinLabel_->setText(state_->pin().isEmpty() ? QStringLiteral("------") : state_->pin());
  playerCountLabel_->setText(QStringLiteral("%1 players connected").arg(competingConnected));
  playerList_->setPlayers(state_->players(), state_->myPlayerId());
  startBtn_->setEnabled(competingConnected >= 1);
}

} // namespace quizlyx::client::ui::pages
