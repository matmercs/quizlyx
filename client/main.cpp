#include <iostream>

#include <boost/filesystem.hpp>

#include <QApplication>
#include <QDateTime>
#include <QLCDNumber>
#include <QTimer>

namespace {
static constexpr int KInterval = 1000;
static constexpr int KDigitCount = 8;
static constexpr int KWidth = 280;
static constexpr int KHeight = 100;
static constexpr const char* KFormat = "hh:mm:ss";
} // namespace

int main(int argc, char** argv) {
  // Boost.Filesystem (requires linking)
  std::cout << "CWD: " << boost::filesystem::current_path().string() << "\n";

  if (argc > 1) {
    std::cout << "No CLI arguments allowed.\n";
    return 0;
  }

  QApplication a(argc, argv);

  QLCDNumber lcd;
  QTimer t;
  QObject::connect(&t, &QTimer::timeout, [&lcd]() { lcd.display(QDateTime::currentDateTime().toString(KFormat)); });
  t.start(KInterval);
  lcd.setDigitCount(KDigitCount);
  lcd.display(QDateTime::currentDateTime().toString(KFormat));
  lcd.resize(KWidth, KHeight);
  lcd.show();

  return QApplication::exec();
}
