#include <QApplication>
#include <QFile>
#include <QTest>

#include "controller/GameController.hpp"
#include "model/SessionState.hpp"
#include "network/NetworkClient.hpp"
#include "ui/MainWindow.hpp"
#include "ui/Theme.hpp"

using namespace quizlyx::client;

class TestUiTheme : public QObject {
  Q_OBJECT

private slots:
  void styleSheetUsesUnifiedVioletAccent();
  void styleSheetDefinesLobbyPlayerSeparators();
  void mainWindowMinimumSizeMatchesLaunchSize();
};

void TestUiTheme::styleSheetUsesUnifiedVioletAccent() {
  ui::applyTheme(*qApp);

  const QString styleSheet = qApp->styleSheet();
  QVERIFY(styleSheet.contains(QStringLiteral("#7C5CFF")));
  QVERIFY(!styleSheet.contains(QStringLiteral("#4C6EF5")));
  QVERIFY(!styleSheet.contains(QStringLiteral("#5F7DFF")));
}

void TestUiTheme::styleSheetDefinesLobbyPlayerSeparators() {
  ui::applyTheme(*qApp);

  const QString styleSheet = qApp->styleSheet();
  QVERIFY(styleSheet.contains(QStringLiteral("QListWidget#playerList::item")));
  QVERIFY(styleSheet.contains(QStringLiteral("border-bottom: 1px solid #31343C;")));
}

void TestUiTheme::mainWindowMinimumSizeMatchesLaunchSize() {
  network::NetworkClient net;
  model::SessionState state;
  controller::GameController controller(&net, &state);
  ui::MainWindow window(&state, &controller);

  QCOMPARE(window.size(), QSize(1200, 980));
  QCOMPARE(window.minimumSize(), window.size());
}

QTEST_MAIN(TestUiTheme)
#include "test_ui_theme.moc"
