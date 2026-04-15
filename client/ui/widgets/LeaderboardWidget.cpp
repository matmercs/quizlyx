#include "ui/widgets/LeaderboardWidget.hpp"

#include <QFontInfo>
#include <QHeaderView>
#include <algorithm>

namespace quizlyx::client::ui::widgets {

LeaderboardWidget::LeaderboardWidget(QWidget* parent) : QTableWidget(parent) {
  setObjectName(QStringLiteral("leaderboard"));
  setColumnCount(3);
  setHorizontalHeaderLabels({QStringLiteral("#"), QStringLiteral("Player"), QStringLiteral("Score")});
  verticalHeader()->setVisible(false);
  horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  setSelectionMode(QAbstractItemView::NoSelection);
  setFocusPolicy(Qt::NoFocus);
  setEditTriggers(QAbstractItemView::NoEditTriggers);
  setShowGrid(false);
  setAlternatingRowColors(true);
  verticalHeader()->setDefaultSectionSize(58);
}

void LeaderboardWidget::setRows(const QVector<model::LeaderboardRow>& rows,
                                const QHash<QString, QString>& displayNameById,
                                const QString& myPlayerId) {
  QFont itemFont = font();
  const QFontInfo resolvedFont(itemFont);
  const int basePixelSize = resolvedFont.pixelSize() > 0 ? resolvedFont.pixelSize() : 14;
  itemFont.setPixelSize(std::max(basePixelSize + 4, 18));
  itemFont.setWeight(QFont::Medium);

  const int rowCount = static_cast<int>(rows.size());
  setRowCount(rowCount);
  for (int i = 0; i < rowCount; ++i) {
    const auto& row = rows.at(i);
    const auto display = displayNameById.value(row.playerId, row.playerId);
    QString label = display;
    if (row.playerId == myPlayerId)
      label += QStringLiteral("  (you)");
    auto* rankItem = new QTableWidgetItem(QString::number(i + 1));
    auto* playerItem = new QTableWidgetItem(label);
    auto* scoreItem = new QTableWidgetItem(QString::number(row.score));

    rankItem->setFont(itemFont);
    playerItem->setFont(itemFont);
    scoreItem->setFont(itemFont);

    setItem(i, 0, rankItem);
    setItem(i, 1, playerItem);
    setItem(i, 2, scoreItem);
  }
}

} // namespace quizlyx::client::ui::widgets
