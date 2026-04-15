#ifndef QUIZLYX_CLIENT_UI_MAIN_WINDOW_HPP
#define QUIZLYX_CLIENT_UI_MAIN_WINDOW_HPP

#include <QMainWindow>

class QStackedWidget;

namespace quizlyx::client::model {
class SessionState;
enum class Phase;
} // namespace quizlyx::client::model

namespace quizlyx::client::controller {
class GameController;
} // namespace quizlyx::client::controller

namespace quizlyx::client::ui::pages {
class ConnectPage;
class RoleSelectPage;
class QuizEditorPage;
class HostLobbyPage;
class JoinPage;
class PlayerLobbyPage;
class QuestionPage;
class LeaderboardPage;
class FinalPage;
} // namespace quizlyx::client::ui::pages

namespace quizlyx::client::ui {

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  MainWindow(model::SessionState* state, controller::GameController* controller, QWidget* parent = nullptr);

private slots:
  void onPhaseChanged(model::Phase phase);
  void onControllerError(const QString& msg);

private:
  enum PageIndex {
    kPageConnect = 0,
    kPageRoleSelect,
    kPageQuizEditor,
    kPageHostLobby,
    kPageJoin,
    kPagePlayerLobby,
    kPageQuestion,
    kPageLeaderboard,
    kPageFinal,
  };

  void showPage(PageIndex index);

  model::SessionState* state_;
  controller::GameController* controller_;

  QStackedWidget* stack_ = nullptr;
  pages::ConnectPage* connectPage_ = nullptr;
  pages::RoleSelectPage* rolePage_ = nullptr;
  pages::QuizEditorPage* editorPage_ = nullptr;
  pages::HostLobbyPage* hostLobbyPage_ = nullptr;
  pages::JoinPage* joinPage_ = nullptr;
  pages::PlayerLobbyPage* playerLobbyPage_ = nullptr;
  pages::QuestionPage* questionPage_ = nullptr;
  pages::LeaderboardPage* leaderboardPage_ = nullptr;
  pages::FinalPage* finalPage_ = nullptr;
};

} // namespace quizlyx::client::ui

#endif // QUIZLYX_CLIENT_UI_MAIN_WINDOW_HPP
