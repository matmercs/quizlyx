#ifndef QUIZLYX_CLIENT_UI_PAGES_FINAL_PAGE_HPP
#define QUIZLYX_CLIENT_UI_PAGES_FINAL_PAGE_HPP

#include <QWidget>

class QLabel;

namespace quizlyx::client::model {
class SessionState;
} // namespace quizlyx::client::model

namespace quizlyx::client::ui::widgets {
class LeaderboardWidget;
} // namespace quizlyx::client::ui::widgets

namespace quizlyx::client::ui::pages {

class FinalPage : public QWidget {
  Q_OBJECT
public:
  explicit FinalPage(model::SessionState* state, QWidget* parent = nullptr);

signals:
  void backToMenu();

private slots:
  void refresh();

private:
  model::SessionState* state_;
  QLabel* winnerLabel_ = nullptr;
  widgets::LeaderboardWidget* board_ = nullptr;
};

} // namespace quizlyx::client::ui::pages

#endif // QUIZLYX_CLIENT_UI_PAGES_FINAL_PAGE_HPP
