#ifndef QUIZLYX_CLIENT_UI_PAGES_HOST_LOBBY_PAGE_HPP
#define QUIZLYX_CLIENT_UI_PAGES_HOST_LOBBY_PAGE_HPP

#include <QWidget>

class QLabel;
class QPushButton;

namespace quizlyx::client::model {
class SessionState;
}

namespace quizlyx::client::ui::widgets {
class PlayerListWidget;
}

namespace quizlyx::client::ui::pages {

class HostLobbyPage : public QWidget {
  Q_OBJECT
public:
  explicit HostLobbyPage(model::SessionState* state, QWidget* parent = nullptr);

signals:
  void startRequested();
  void leaveRequested();

private slots:
  void refresh();

private:
  model::SessionState* state_;
  QLabel* pinLabel_ = nullptr;
  QLabel* playerCountLabel_ = nullptr;
  QPushButton* startBtn_ = nullptr;
  widgets::PlayerListWidget* playerList_ = nullptr;
};

} // namespace quizlyx::client::ui::pages

#endif // QUIZLYX_CLIENT_UI_PAGES_HOST_LOBBY_PAGE_HPP
