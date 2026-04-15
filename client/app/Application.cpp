#include "app/Application.hpp"

#include <QMetaType>

#include "controller/GameController.hpp"
#include "model/Protocol.hpp"
#include "model/SessionState.hpp"
#include "network/NetworkClient.hpp"
#include "ui/MainWindow.hpp"
#include "ui/Theme.hpp"

namespace quizlyx::client::app {

namespace {

void registerMetaTypes() {
  static bool done = false;
  if (done)
    return;
  done = true;
  qRegisterMetaType<quizlyx::client::model::QuestionStartedEv>("quizlyx::client::model::QuestionStartedEv");
  qRegisterMetaType<quizlyx::client::model::TimerUpdateEv>("quizlyx::client::model::TimerUpdateEv");
  qRegisterMetaType<quizlyx::client::model::QuestionTimeoutEv>("quizlyx::client::model::QuestionTimeoutEv");
  qRegisterMetaType<quizlyx::client::model::LeaderboardEv>("quizlyx::client::model::LeaderboardEv");
  qRegisterMetaType<quizlyx::client::model::PlayerJoinedEv>("quizlyx::client::model::PlayerJoinedEv");
  qRegisterMetaType<quizlyx::client::model::PlayerLeftEv>("quizlyx::client::model::PlayerLeftEv");
  qRegisterMetaType<quizlyx::client::model::GameFinishedEv>("quizlyx::client::model::GameFinishedEv");
  qRegisterMetaType<quizlyx::client::model::Quiz>("quizlyx::client::model::Quiz");
  qRegisterMetaType<quizlyx::client::model::Question>("quizlyx::client::model::Question");
  qRegisterMetaType<quizlyx::client::model::Phase>("quizlyx::client::model::Phase");
}

} // namespace

Application::Application(QObject* parent) : QObject(parent) {
  registerMetaTypes();

  state_ = std::make_unique<model::SessionState>();

  networkThread_ = std::make_unique<QThread>();
  networkThread_->setObjectName(QStringLiteral("quizlyx-net"));
  networkClient_ = new network::NetworkClient;
  networkClient_->moveToThread(networkThread_.get());

  connect(networkThread_.get(), &QThread::started, networkClient_, &network::NetworkClient::initialize);
  connect(networkThread_.get(), &QThread::finished, networkClient_, &QObject::deleteLater);
  networkThread_->start();

  controller_ = std::make_unique<controller::GameController>(networkClient_, state_.get());
  mainWindow_ = std::make_unique<ui::MainWindow>(state_.get(), controller_.get());
}

Application::~Application() {
  if (networkThread_) {
    QMetaObject::invokeMethod(networkClient_, "disconnectFromServer", Qt::BlockingQueuedConnection);
    networkThread_->quit();
    networkThread_->wait();
  }
}

void Application::show() {
  mainWindow_->show();
}

} // namespace quizlyx::client::app
