#include "ui/MainWindow.hpp"

#include <QMessageBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "controller/GameController.hpp"
#include "model/SessionState.hpp"
#include "ui/pages/ConnectPage.hpp"
#include "ui/pages/FinalPage.hpp"
#include "ui/pages/HostLobbyPage.hpp"
#include "ui/pages/JoinPage.hpp"
#include "ui/pages/LeaderboardPage.hpp"
#include "ui/pages/PlayerLobbyPage.hpp"
#include "ui/pages/QuestionPage.hpp"
#include "ui/pages/QuizEditorPage.hpp"
#include "ui/pages/RoleSelectPage.hpp"

namespace quizlyx::client::ui {

MainWindow::MainWindow(model::SessionState* state, controller::GameController* controller, QWidget* parent) :
    QMainWindow(parent), state_(state), controller_(controller) {
  setWindowTitle(QStringLiteral("Quizlyx"));
  resize(1080, 760);
  setMinimumSize(1080, 760);

  auto* root = new QWidget(this);
  root->setObjectName(QStringLiteral("centralRoot"));
  setCentralWidget(root);

  stack_ = new QStackedWidget(root);
  auto* layout = new QVBoxLayout(root);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(stack_);

  connectPage_ = new pages::ConnectPage;
  rolePage_ = new pages::RoleSelectPage;
  editorPage_ = new pages::QuizEditorPage;
  hostLobbyPage_ = new pages::HostLobbyPage(state_);
  joinPage_ = new pages::JoinPage;
  playerLobbyPage_ = new pages::PlayerLobbyPage(state_);
  questionPage_ = new pages::QuestionPage(state_);
  leaderboardPage_ = new pages::LeaderboardPage(state_);
  finalPage_ = new pages::FinalPage(state_);

  stack_->insertWidget(kPageConnect, connectPage_);
  stack_->insertWidget(kPageRoleSelect, rolePage_);
  stack_->insertWidget(kPageQuizEditor, editorPage_);
  stack_->insertWidget(kPageHostLobby, hostLobbyPage_);
  stack_->insertWidget(kPageJoin, joinPage_);
  stack_->insertWidget(kPagePlayerLobby, playerLobbyPage_);
  stack_->insertWidget(kPageQuestion, questionPage_);
  stack_->insertWidget(kPageLeaderboard, leaderboardPage_);
  stack_->insertWidget(kPageFinal, finalPage_);
  stack_->setCurrentIndex(kPageConnect);

  connect(
      connectPage_, &pages::ConnectPage::connectRequested, controller_, &controller::GameController::connectToServer);
  connect(rolePage_, &pages::RoleSelectPage::hostSelected, this, [this]() { showPage(kPageQuizEditor); });
  connect(rolePage_, &pages::RoleSelectPage::joinSelected, this, [this]() {
    joinPage_->resetUiState();
    showPage(kPageJoin);
  });

  connect(editorPage_, &pages::QuizEditorPage::hostRequested, controller_, &controller::GameController::hostGame);
  connect(editorPage_, &pages::QuizEditorPage::backRequested, this, [this]() { showPage(kPageRoleSelect); });

  connect(joinPage_, &pages::JoinPage::joinRequested, controller_, &controller::GameController::joinGame);
  connect(joinPage_, &pages::JoinPage::backRequested, this, [this]() {
    joinPage_->resetUiState();
    showPage(kPageRoleSelect);
  });

  connect(hostLobbyPage_, &pages::HostLobbyPage::startRequested, controller_, &controller::GameController::startGame);
  connect(
      hostLobbyPage_, &pages::HostLobbyPage::leaveRequested, controller_, &controller::GameController::leaveSession);

  connect(playerLobbyPage_,
          &pages::PlayerLobbyPage::leaveRequested,
          controller_,
          &controller::GameController::leaveSession);

  connect(questionPage_, &pages::QuestionPage::submitAnswer, controller_, &controller::GameController::submitAnswer);

  connect(leaderboardPage_,
          &pages::LeaderboardPage::nextQuestionRequested,
          controller_,
          &controller::GameController::requestNextQuestion);
  connect(leaderboardPage_,
          &pages::LeaderboardPage::leaveRequested,
          controller_,
          &controller::GameController::leaveSession);

  connect(finalPage_, &pages::FinalPage::backToMenu, controller_, &controller::GameController::leaveSession);

  connect(state_, &model::SessionState::phaseChanged, this, &MainWindow::onPhaseChanged);
  connect(controller_, &controller::GameController::errorOccurred, this, &MainWindow::onControllerError);
  connect(controller_, &controller::GameController::sessionCreated, this, [this](QString, QString) {
    showPage(kPageHostLobby);
  });
  connect(controller_, &controller::GameController::joinedSession, this, [this](QString, QString) {
    joinPage_->resetUiState();
    showPage(kPagePlayerLobby);
  });
}

void MainWindow::onPhaseChanged(model::Phase phase) {
  switch (phase) {
    case model::Phase::Disconnected:
      showPage(kPageConnect);
      connectPage_->resetUiState();
      break;
    case model::Phase::Connecting:
      connectPage_->setBusy(true);
      break;
    case model::Phase::Connected:
      connectPage_->resetUiState();
      joinPage_->resetUiState();
      showPage(kPageRoleSelect);
      break;
    case model::Phase::Lobby:
      if (state_->role() == model::Role::Host)
        showPage(kPageHostLobby);
      else
        showPage(kPagePlayerLobby);
      break;
    case model::Phase::Question:
      showPage(kPageQuestion);
      break;
    case model::Phase::Leaderboard:
      showPage(kPageLeaderboard);
      break;
    case model::Phase::Finished:
      showPage(kPageFinal);
      break;
    case model::Phase::Reconnecting:
      break;
  }
}

void MainWindow::onControllerError(const QString& msg) {
  if (stack_->currentIndex() == kPageConnect) {
    connectPage_->setBusy(false);
    connectPage_->setStatus(msg);
  } else if (stack_->currentIndex() == kPageJoin) {
    joinPage_->setBusy(false);
    joinPage_->setStatus(msg);
  }
  QMessageBox::warning(this, QStringLiteral("Quizlyx"), msg);
}

void MainWindow::showPage(PageIndex index) {
  stack_->setCurrentIndex(index);
}

} // namespace quizlyx::client::ui
