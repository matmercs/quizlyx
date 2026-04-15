#include <QApplication>

#include "app/Application.hpp"
#include "ui/Theme.hpp"

int main(int argc, char** argv) {
  QApplication qt(argc, argv);
  QApplication::setApplicationName(QStringLiteral("Quizlyx"));
  QApplication::setOrganizationName(QStringLiteral("Quizlyx"));

  quizlyx::client::ui::applyTheme(qt);

  quizlyx::client::app::Application app;
  app.show();

  return QApplication::exec();
}
