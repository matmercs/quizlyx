#include "ui/pages/JoinPage.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace quizlyx::client::ui::pages {

JoinPage::JoinPage(QWidget* parent) : QWidget(parent) {
  constexpr int kFieldWidth = 240;
  constexpr int kActionButtonWidth = 112;

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(40, 60, 40, 60);
  root->setSpacing(20);
  root->addStretch(1);

  auto* title = new QLabel(QStringLiteral("Join a game"));
  title->setObjectName(QStringLiteral("titleLabel"));
  title->setAlignment(Qt::AlignCenter);
  root->addWidget(title);

  pinEdit_ = new QLineEdit;
  pinEdit_->setPlaceholderText(QStringLiteral("PIN"));
  pinEdit_->setMaxLength(6);
  nameEdit_ = new QLineEdit;
  nameEdit_->setPlaceholderText(QStringLiteral("your name"));
  pinEdit_->setFixedWidth(kFieldWidth);
  nameEdit_->setFixedWidth(kFieldWidth);
  root->addWidget(pinEdit_, 0, Qt::AlignHCenter);
  root->addWidget(nameEdit_, 0, Qt::AlignHCenter);

  joinBtn_ = new QPushButton(QStringLiteral("Join"));
  joinBtn_->setObjectName(QStringLiteral("primaryButton"));
  joinBtn_->setDefault(true);

  statusLabel_ = new QLabel;
  statusLabel_->setObjectName(QStringLiteral("statusLabel"));
  statusLabel_->setAlignment(Qt::AlignCenter);

  auto* backBtn = new QPushButton(QStringLiteral("← Back"));
  const int actionButtonHeight = pinEdit_->sizeHint().height();
  backBtn->setFixedSize(kActionButtonWidth, actionButtonHeight);
  joinBtn_->setFixedSize(kActionButtonWidth, actionButtonHeight);

  auto* btnRow = new QHBoxLayout;
  btnRow->setSpacing(12);
  btnRow->setAlignment(Qt::AlignHCenter);
  btnRow->addWidget(backBtn);
  btnRow->addWidget(joinBtn_);
  root->addLayout(btnRow);

  root->addWidget(statusLabel_);
  root->addStretch(2);

  connect(joinBtn_, &QPushButton::clicked, this, &JoinPage::onJoinClicked);
  connect(backBtn, &QPushButton::clicked, this, &JoinPage::backRequested);
  connect(pinEdit_, &QLineEdit::returnPressed, this, &JoinPage::onJoinClicked);
  connect(nameEdit_, &QLineEdit::returnPressed, this, &JoinPage::onJoinClicked);
}

void JoinPage::setStatus(const QString& text) {
  statusLabel_->setText(text);
}

void JoinPage::setBusy(bool busy) {
  joinBtn_->setEnabled(!busy);
  pinEdit_->setEnabled(!busy);
  nameEdit_->setEnabled(!busy);
}

void JoinPage::resetUiState() {
  setBusy(false);
  setStatus(QString());
}

void JoinPage::onJoinClicked() {
  const auto pin = pinEdit_->text().trimmed();
  const auto name = nameEdit_->text().trimmed();
  if (pin.isEmpty() || name.isEmpty()) {
    setStatus(QStringLiteral("All fields are required."));
    return;
  }
  setBusy(true);
  setStatus(QStringLiteral("Joining…"));
  emit joinRequested(pin, name);
}

} // namespace quizlyx::client::ui::pages
