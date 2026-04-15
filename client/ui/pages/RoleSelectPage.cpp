#include "ui/pages/RoleSelectPage.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace quizlyx::client::ui::pages {

namespace {

QPushButton* buildRoleButton(const QString& title, const QString& subtitle) {
  auto* btn = new QPushButton;
  btn->setFixedSize(260, 208);
  btn->setCursor(Qt::PointingHandCursor);
  btn->setText(QStringLiteral("%1\n\n%2").arg(title, subtitle));
  btn->setObjectName(QStringLiteral("roleButton"));
  return btn;
}

} // namespace

RoleSelectPage::RoleSelectPage(QWidget* parent) : QWidget(parent) {
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(40, 60, 40, 60);
  root->setSpacing(24);
  root->addStretch(1);

  auto* title = new QLabel(QStringLiteral("What do you want to do?"));
  title->setObjectName(QStringLiteral("titleLabel"));
  title->setAlignment(Qt::AlignCenter);
  root->addWidget(title);

  auto* row = new QHBoxLayout;
  row->setSpacing(24);
  row->addStretch(1);
  auto* hostBtn = buildRoleButton(QStringLiteral("Host a game"), QStringLiteral("Create a quiz,\nRun the lobby."));
  auto* joinBtn = buildRoleButton(QStringLiteral("Join a game"), QStringLiteral("Play with a PIN."));
  row->addWidget(hostBtn);
  row->addWidget(joinBtn);
  row->addStretch(1);
  root->addLayout(row);

  root->addStretch(2);

  connect(hostBtn, &QPushButton::clicked, this, &RoleSelectPage::hostSelected);
  connect(joinBtn, &QPushButton::clicked, this, &RoleSelectPage::joinSelected);
}

} // namespace quizlyx::client::ui::pages
