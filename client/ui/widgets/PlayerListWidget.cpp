#include "ui/widgets/PlayerListWidget.hpp"

#include <QFontInfo>

namespace quizlyx::client::ui::widgets {

PlayerListWidget::PlayerListWidget(QWidget* parent) : QListWidget(parent) {
  setObjectName(QStringLiteral("playerList"));
  setFocusPolicy(Qt::NoFocus);
  setSelectionMode(QAbstractItemView::NoSelection);
  setFrameShape(QFrame::NoFrame);
  setSpacing(0);
}

void PlayerListWidget::setPlayers(const QVector<model::PlayerEntry>& players, const QString& myPlayerId) {
  clear();
  QFont itemFont = font();
  const QFontInfo resolvedFont(itemFont);
  const int basePixelSize = resolvedFont.pixelSize() > 0 ? resolvedFont.pixelSize() : 14;
  itemFont.setPixelSize(std::max(basePixelSize + 4, 18));
  itemFont.setWeight(QFont::Medium);

  for (const auto& p : players) {
    QString label = p.name.isEmpty() ? p.id : p.name;
    if (p.isHost)
      label += QStringLiteral("  (host)");
    if (!p.isCompeting)
      label += QStringLiteral("  spectator");
    if (p.id == myPlayerId)
      label += QStringLiteral("  (you)");
    if (!p.connected)
      label += QStringLiteral("  disconnected");

    auto* item = new QListWidgetItem(label);
    item->setFont(itemFont);
    item->setSizeHint(QSize(0, 58));
    addItem(item);
  }
}

} // namespace quizlyx::client::ui::widgets
