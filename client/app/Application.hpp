#ifndef QUIZLYX_CLIENT_APP_APPLICATION_HPP
#define QUIZLYX_CLIENT_APP_APPLICATION_HPP

#include <QObject>
#include <QThread>
#include <memory>

namespace quizlyx::client::model {
class SessionState;
} // namespace quizlyx::client::model

namespace quizlyx::client::network {
class NetworkClient;
} // namespace quizlyx::client::network

namespace quizlyx::client::controller {
class GameController;
} // namespace quizlyx::client::controller

namespace quizlyx::client::ui {
class MainWindow;
} // namespace quizlyx::client::ui

namespace quizlyx::client::app {

class Application : public QObject {
  Q_OBJECT
public:
  explicit Application(QObject* parent = nullptr);
  ~Application() override;

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  Application(Application&&) = delete;
  Application& operator=(Application&&) = delete;

  void show();

private:
  std::unique_ptr<QThread> networkThread_;
  network::NetworkClient* networkClient_ = nullptr;
  std::unique_ptr<model::SessionState> state_;
  std::unique_ptr<controller::GameController> controller_;
  std::unique_ptr<ui::MainWindow> mainWindow_;
};

} // namespace quizlyx::client::app

#endif // QUIZLYX_CLIENT_APP_APPLICATION_HPP
