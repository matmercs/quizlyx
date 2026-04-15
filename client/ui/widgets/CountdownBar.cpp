#include "ui/widgets/CountdownBar.hpp"

#include <QPaintEvent>
#include <QPainter>
#include <algorithm>

namespace quizlyx::client::ui::widgets {

namespace {

QColor interpolate(const QColor& start, const QColor& end, double ratio) {
  const auto lerp = [ratio](int from, int to) { return static_cast<int>(from * ratio + to * (1.0 - ratio)); };
  return QColor(lerp(start.red(), end.red()), lerp(start.green(), end.green()), lerp(start.blue(), end.blue()));
}

} // namespace

CountdownBar::CountdownBar(QWidget* parent) : QWidget(parent) {
  setMinimumHeight(26);
  setMaximumHeight(32);
  tick_.setInterval(16);
  connect(&tick_, &QTimer::timeout, this, &CountdownBar::onTick);
}

void CountdownBar::setDuration(int durationMs) {
  durationMs_ = std::max(1, durationMs);
  lastServerRemainingMs_ = durationMs_;
  sinceLastServerUpdate_.restart();
  active_ = true;
  tick_.start();
  update();
}

void CountdownBar::setRemainingMs(int remainingMs) {
  lastServerRemainingMs_ = std::max(0, remainingMs);
  sinceLastServerUpdate_.restart();
  if (lastServerRemainingMs_ == 0) {
    stop();
  } else {
    active_ = true;
    if (!tick_.isActive())
      tick_.start();
  }
  update();
}

void CountdownBar::stop() {
  active_ = false;
  tick_.stop();
  update();
}

void CountdownBar::onTick() {
  update();
}

void CountdownBar::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  int interpRemaining = lastServerRemainingMs_;
  if (active_ && sinceLastServerUpdate_.isValid()) {
    interpRemaining = std::max(0, lastServerRemainingMs_ - static_cast<int>(sinceLastServerUpdate_.elapsed()));
  }

  const auto rect = this->rect().adjusted(0, 0, -1, -1);
  p.setPen(QPen(QColor(255, 255, 255, 40), 1));
  p.setBrush(QColor(255, 255, 255, 15));
  p.drawRoundedRect(rect, 8, 8);

  const double ratio = durationMs_ > 0 ? std::clamp(static_cast<double>(interpRemaining) / durationMs_, 0.0, 1.0) : 0.0;
  auto fillRect = rect;
  fillRect.setWidth(static_cast<int>(ratio * rect.width()));

  const QColor fill = interpolate(QColor(245, 245, 247), QColor(196, 54, 64), ratio);
  p.setPen(Qt::NoPen);
  p.setBrush(fill);
  p.drawRoundedRect(fillRect, 8, 8);

  p.setPen(QColor(237, 237, 237));
  QFont f = p.font();
  if (f.pointSizeF() > 0)
    f.setPointSizeF(f.pointSizeF() + 1);
  else if (f.pixelSize() > 0)
    f.setPixelSize(f.pixelSize() + 1);
  f.setBold(true);
  p.setFont(f);
  const auto label = QString::number((interpRemaining + 500) / 1000) + QStringLiteral(" s");
  p.drawText(rect, Qt::AlignCenter, label);
}

} // namespace quizlyx::client::ui::widgets
