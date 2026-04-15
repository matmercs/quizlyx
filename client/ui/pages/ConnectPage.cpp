#include "ui/pages/ConnectPage.hpp"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace quizlyx::client::ui::pages {

ConnectPage::ConnectPage(QWidget* parent) : QWidget(parent) {
  constexpr int kLabelWidth = 60;
  constexpr int kFieldWidth = 220;

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(40, 60, 40, 60);
  root->setSpacing(24);
  root->addStretch(1);

  auto* title = new QLabel(QStringLiteral("Quizlyx"));
  title->setObjectName(QStringLiteral("titleLabel"));
  title->setAlignment(Qt::AlignCenter);

  auto* subtitle = new QLabel(QStringLiteral("Connect to a Quizlyx server"));
  subtitle->setObjectName(QStringLiteral("subtitleLabel"));
  subtitle->setAlignment(Qt::AlignCenter);

  hostEdit_ = new QLineEdit(QStringLiteral("127.0.0.1"));
  hostEdit_->setPlaceholderText(QStringLiteral("host"));
  portEdit_ = new QLineEdit(QStringLiteral("8080"));
  portEdit_->setPlaceholderText(QStringLiteral("port"));
  portEdit_->setValidator(new QIntValidator(1, 65535, this));
  hostEdit_->setFixedWidth(kFieldWidth);
  portEdit_->setFixedWidth(kFieldWidth);

  auto* hostLabel = new QLabel(QStringLiteral("Host"));
  hostLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  hostLabel->setFixedWidth(kLabelWidth);

  auto* portLabel = new QLabel(QStringLiteral("Port"));
  portLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  portLabel->setFixedWidth(kLabelWidth);

  auto* formWidget = new QWidget;
  auto* form = new QGridLayout(formWidget);
  form->setContentsMargins(0, 0, 0, 0);
  form->setHorizontalSpacing(14);
  form->setVerticalSpacing(16);
  form->setColumnMinimumWidth(0, kLabelWidth);
  form->setColumnMinimumWidth(2, kLabelWidth);
  form->addWidget(hostLabel, 0, 0);
  form->addWidget(hostEdit_, 0, 1);
  form->addWidget(portLabel, 1, 0);
  form->addWidget(portEdit_, 1, 1);

  connectBtn_ = new QPushButton(QStringLiteral("Connect"));
  connectBtn_->setObjectName(QStringLiteral("primaryButton"));
  connectBtn_->setDefault(true);

  statusLabel_ = new QLabel;
  statusLabel_->setObjectName(QStringLiteral("statusLabel"));
  statusLabel_->setAlignment(Qt::AlignCenter);

  root->addWidget(title);
  root->addWidget(subtitle);
  root->addSpacing(10);
  root->addWidget(formWidget, 0, Qt::AlignHCenter);
  root->addWidget(connectBtn_, 0, Qt::AlignHCenter);
  root->addWidget(statusLabel_);
  root->addStretch(2);

  connect(connectBtn_, &QPushButton::clicked, this, &ConnectPage::onConnectClicked);
  connect(hostEdit_, &QLineEdit::returnPressed, this, &ConnectPage::onConnectClicked);
  connect(portEdit_, &QLineEdit::returnPressed, this, &ConnectPage::onConnectClicked);
}

void ConnectPage::setStatus(const QString& text) {
  statusLabel_->setText(text);
}

void ConnectPage::setBusy(bool busy) {
  connectBtn_->setEnabled(!busy);
  hostEdit_->setEnabled(!busy);
  portEdit_->setEnabled(!busy);
}

void ConnectPage::resetUiState() {
  setBusy(false);
  setStatus(QString());
}

void ConnectPage::onConnectClicked() {
  const auto host = hostEdit_->text().trimmed();
  const auto port = static_cast<quint16>(portEdit_->text().toUInt());
  if (host.isEmpty() || port == 0) {
    setStatus(QStringLiteral("Enter a valid host and port."));
    return;
  }
  setStatus(QStringLiteral("Connecting…"));
  emit connectRequested(host, port);
}

} // namespace quizlyx::client::ui::pages
