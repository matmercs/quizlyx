#ifndef QUIZLYX_CLIENT_UI_PAGES_LEADERBOARD_PAGE_HPP
#define QUIZLYX_CLIENT_UI_PAGES_LEADERBOARD_PAGE_HPP

#include <QTimer>
#include <QWidget>

class QLabel;
class QPushButton;

namespace quizlyx::client::model {
class SessionState;
} // namespace quizlyx::client::model

namespace quizlyx::client::ui::widgets {
class LeaderboardWidget;
} // namespace quizlyx::client::ui::widgets

namespace quizlyx::client::ui::pages {

class LeaderboardPage : public QWidget {
  Q_OBJECT
public:
  explicit LeaderboardPage(model::SessionState* state, QWidget* parent = nullptr);

signals:
  void nextQuestionRequested();
  void leaveRequested();

private slots:
  void refresh();
  void updateCountdown();

private:
  model::SessionState* state_;
  QLabel* titleLabel_ = nullptr;
  QLabel* countdownLabel_ = nullptr;
  widgets::LeaderboardWidget* board_ = nullptr;
  QPushButton* nextBtn_ = nullptr;
  QTimer countdownTimer_;
  qint64 countdownDeadlineMs_ = 0;
};

} // namespace quizlyx::client::ui::pages

#endif // QUIZLYX_CLIENT_UI_PAGES_LEADERBOARD_PAGE_HPP
