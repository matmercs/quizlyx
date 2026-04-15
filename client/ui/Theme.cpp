#include "ui/Theme.hpp"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QPalette>

namespace quizlyx::client::ui {

void applyTheme(QApplication& app) {
  QPalette p;
  const QColor bg(30, 31, 34);
  const QColor bgAlt(36, 38, 42);
  const QColor fg(237, 237, 237);
  const QColor accent(124, 92, 255);

  p.setColor(QPalette::Window, bg);
  p.setColor(QPalette::Base, bgAlt);
  p.setColor(QPalette::AlternateBase, QColor(42, 45, 50));
  p.setColor(QPalette::Button, bgAlt);
  p.setColor(QPalette::ButtonText, fg);
  p.setColor(QPalette::WindowText, fg);
  p.setColor(QPalette::Text, fg);
  p.setColor(QPalette::ToolTipBase, bgAlt);
  p.setColor(QPalette::ToolTipText, fg);
  p.setColor(QPalette::Highlight, accent);
  p.setColor(QPalette::HighlightedText, Qt::white);
  p.setColor(QPalette::Link, accent);
  p.setColor(QPalette::PlaceholderText, QColor(160, 160, 160));
  app.setPalette(p);

  QFile f(QStringLiteral(":/client/resources/style.qss"));
  if (f.open(QIODevice::ReadOnly | QIODevice::Text))
    app.setStyleSheet(QString::fromUtf8(f.readAll()));
}

} // namespace quizlyx::client::ui
