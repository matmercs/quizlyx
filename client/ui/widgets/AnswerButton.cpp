#include "ui/widgets/AnswerButton.hpp"

#include <QColor>
#include <QLabel>
#include <QVBoxLayout>

namespace quizlyx::client::ui::widgets {

namespace {

constexpr const char* kTones[] = {"#2C3036", "#2F333A", "#32363E", "#353A42"};

QColor toneFor(int index) {
  return QColor(QString::fromLatin1(kTones[index % 4]));
}

QString shapeFor(int index) {
  static constexpr const char* kShapes[] = {"▲", "◆", "●", "■"};
  return QString::fromUtf8(kShapes[index % 4]);
}

QString countLabel(int count) {
  return count == 1 ? QStringLiteral("Chosen by 1 player") : QStringLiteral("Chosen by %1 players").arg(count);
}

} // namespace

AnswerButton::AnswerButton(int index, QString text, QWidget* parent) :
    QPushButton(parent), index_(index), text_(std::move(text)) {
  setMinimumHeight(168);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  setCursor(Qt::PointingHandCursor);
  setFocusPolicy(Qt::StrongFocus);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(18, 18, 18, 18);
  layout->setSpacing(0);
  layout->addStretch(1);

  markerLabel_ = new QLabel(this);
  markerLabel_->setAlignment(Qt::AlignCenter);
  markerLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
  layout->addWidget(markerLabel_);

  layout->addSpacing(6);

  optionLabel_ = new QLabel(this);
  optionLabel_->setAlignment(Qt::AlignCenter);
  optionLabel_->setWordWrap(true);
  optionLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
  layout->addWidget(optionLabel_);

  secondaryGap_ = new QWidget(this);
  secondaryGap_->setFixedHeight(18);
  secondaryGap_->setAttribute(Qt::WA_TransparentForMouseEvents);
  layout->addWidget(secondaryGap_);

  countLabel_ = new QLabel(this);
  countLabel_->setAlignment(Qt::AlignCenter);
  countLabel_->setWordWrap(true);
  countLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
  layout->addWidget(countLabel_);

  revealPrimaryLabel_ = new QLabel(this);
  revealPrimaryLabel_->setAlignment(Qt::AlignCenter);
  revealPrimaryLabel_->setWordWrap(true);
  revealPrimaryLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
  layout->addWidget(revealPrimaryLabel_);

  revealSecondaryLabel_ = new QLabel(this);
  revealSecondaryLabel_->setAlignment(Qt::AlignCenter);
  revealSecondaryLabel_->setWordWrap(true);
  revealSecondaryLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
  layout->addWidget(revealSecondaryLabel_);

  layout->addStretch(1);

  connect(this, &QPushButton::clicked, this, [this]() {
    if (locked_)
      return;
    emit answerClicked(index_);
  });
  updateContent();
  applyStyle();
}

void AnswerButton::setChecked(bool checked) {
  selected_ = checked;
  updateContent();
  applyStyle();
}

void AnswerButton::setRevealState(RevealState revealState) {
  revealState_ = revealState;
  updateContent();
  applyStyle();
}

void AnswerButton::setPickCount(int pickCount) {
  pickCount_ = pickCount;
  updateContent();
}

void AnswerButton::showPickCount(bool show) {
  showPickCount_ = show;
  updateContent();
}

void AnswerButton::setLocked(bool locked) {
  locked_ = locked;
  setEnabled(!locked_);
  updateContent();
  applyStyle();
}

void AnswerButton::updateContent() {
  markerLabel_->setText(shapeFor(index_));
  optionLabel_->setText(text_);

  const bool showCount = showPickCount_;
  const bool showSelectedHint = selected_ && locked_ && revealState_ == RevealState::None;
  const bool showCorrectLabels = revealState_ == RevealState::Correct;
  const bool showWrongLabel = revealState_ == RevealState::WrongSelected;

  secondaryGap_->setVisible(showCount || showSelectedHint || showCorrectLabels || showWrongLabel);

  countLabel_->setVisible(showCount);
  countLabel_->setText(showCount ? countLabel(pickCount_) : QString());

  if (showCorrectLabels && selected_) {
    revealPrimaryLabel_->setVisible(true);
    revealPrimaryLabel_->setText(QStringLiteral("Your answer"));
    revealSecondaryLabel_->setVisible(true);
    revealSecondaryLabel_->setText(QStringLiteral("Correct answer"));
  } else if (showCorrectLabels) {
    revealPrimaryLabel_->setVisible(true);
    revealPrimaryLabel_->setText(QStringLiteral("Correct answer"));
    revealSecondaryLabel_->setVisible(false);
    revealSecondaryLabel_->clear();
  } else if (showWrongLabel) {
    revealPrimaryLabel_->setVisible(true);
    revealPrimaryLabel_->setText(QStringLiteral("Your answer"));
    revealSecondaryLabel_->setVisible(false);
    revealSecondaryLabel_->clear();
  } else if (showSelectedHint) {
    revealPrimaryLabel_->setVisible(true);
    revealPrimaryLabel_->setText(QStringLiteral("Selected"));
    revealSecondaryLabel_->setVisible(false);
    revealSecondaryLabel_->clear();
  } else {
    revealPrimaryLabel_->setVisible(false);
    revealPrimaryLabel_->clear();
    revealSecondaryLabel_->setVisible(false);
    revealSecondaryLabel_->clear();
  }
}

void AnswerButton::applyStyle() {
  auto background = toneFor(index_);
  QColor border(58, 62, 70);
  QColor markerColor(231, 233, 236);
  QColor optionColor(237, 237, 237);
  QColor mutedColor(156, 160, 169);

  if (selected_) {
    background = background.lighter(110);
    border = QColor(224, 228, 234);
  }

  QColor revealPrimaryColor = mutedColor;
  QColor revealSecondaryColor = mutedColor;

  if (revealState_ == RevealState::Correct && selected_) {
    const QColor green(93, 214, 126);
    border = green;
    revealPrimaryColor = green;
    revealSecondaryColor = green;
  } else if (revealState_ == RevealState::Correct) {
    const QColor purple(124, 92, 255);
    border = purple;
    revealPrimaryColor = purple;
  } else if (revealState_ == RevealState::WrongSelected) {
    const QColor red(236, 96, 109);
    border = red;
    revealPrimaryColor = red;
  } else if (locked_ && !selected_ && revealState_ == RevealState::None) {
    background = background.darker(108);
    mutedColor = QColor(128, 133, 143);
  } else if (selected_ && locked_) {
    revealPrimaryColor = mutedColor;
  }

  const auto pressed = background.lighter(105).name();
  setStyleSheet(QString(R"(
QPushButton {
  background-color: %1;
  border-radius: 16px;
  border: 2px solid %2;
  padding: 0px;
}
QPushButton:hover { background-color: %3; }
QPushButton:pressed { background-color: %3; }
QPushButton:disabled {
  background-color: %1;
  border: 2px solid %2;
}
)")
                    .arg(background.name())
                    .arg(border.name())
                    .arg(pressed));

  markerLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 18px; font-weight: 700;").arg(markerColor.name()));
  optionLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 20px; font-weight: 700;").arg(optionColor.name()));
  countLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 11px; font-weight: 500;").arg(mutedColor.name()));
  revealPrimaryLabel_->setStyleSheet(
      QStringLiteral("color: %1; font-size: 11px; font-weight: 600;").arg(revealPrimaryColor.name()));
  revealSecondaryLabel_->setStyleSheet(
      QStringLiteral("color: %1; font-size: 11px; font-weight: 600;").arg(revealSecondaryColor.name()));
}

} // namespace quizlyx::client::ui::widgets
