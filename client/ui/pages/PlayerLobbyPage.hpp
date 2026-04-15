#ifndef QUIZLYX_CLIENT_UI_PAGES_PLAYER_LOBBY_PAGE_HPP
#define QUIZLYX_CLIENT_UI_PAGES_PLAYER_LOBBY_PAGE_HPP

#include <QWidget>

class QLabel;
class QPushButton;

namespace quizlyx::client::model {
class SessionState;
} // namespace quizlyx::client::model

namespace quizlyx::client::ui::widgets {
class PlayerListWidget;
} // namespace quizlyx::client::ui::widgets

namespace quizlyx::client::ui::pages {

class PlayerLobbyPage : public QWidget {
  Q_OBJECT
public:
  explicit PlayerLobbyPage(model::SessionState* state, QWidget* parent = nullptr);

signals:
  void leaveRequested();

private slots:
  void refresh();

private:
  model::SessionState* state_;
  QLabel* greetingLabel_ = nullptr;
  QLabel* countLabel_ = nullptr;
  widgets::PlayerListWidget* playerList_ = nullptr;
};

} // namespace quizlyx::client::ui::pages

#endif // QUIZLYX_CLIENT_UI_PAGES_PLAYER_LOBBY_PAGE_HPP
