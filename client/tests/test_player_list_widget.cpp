#include <QFontInfo>
#include <QTest>

#include "model/PlayerEntry.hpp"
#include "ui/Theme.hpp"
#include "ui/widgets/PlayerListWidget.hpp"

using namespace quizlyx::client;

class TestPlayerListWidget : public QObject {
  Q_OBJECT

private slots:
  void usesLargerLobbyRows();
};

void TestPlayerListWidget::usesLargerLobbyRows() {
  ui::applyTheme(*qApp);
  ui::widgets::PlayerListWidget widget;
  QFont baseFont = widget.font();
  baseFont.setPixelSize(14);
  widget.setFont(baseFont);

  QVector<model::PlayerEntry> players;
  players.push_back({QStringLiteral("p1"), QStringLiteral("Alice"), 0, false, true, true});

  widget.setPlayers(players, QStringLiteral("p1"));

  QCOMPARE(widget.count(), 1);
  auto* item = widget.item(0);
  QVERIFY(item != nullptr);
  QVERIFY(item->sizeHint().height() >= 56);
  QVERIFY(QFontInfo(item->font()).pixelSize() >= 18);
}

QTEST_MAIN(TestPlayerListWidget)
#include "test_player_list_widget.moc"
