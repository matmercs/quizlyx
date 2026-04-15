#ifndef QUIZLYX_CLIENT_UI_PAGES_ROLE_SELECT_PAGE_HPP
#define QUIZLYX_CLIENT_UI_PAGES_ROLE_SELECT_PAGE_HPP

#include <QWidget>

namespace quizlyx::client::ui::pages {

class RoleSelectPage : public QWidget {
  Q_OBJECT
public:
  explicit RoleSelectPage(QWidget* parent = nullptr);

signals:
  void hostSelected();
  void joinSelected();
};

} // namespace quizlyx::client::ui::pages

#endif // QUIZLYX_CLIENT_UI_PAGES_ROLE_SELECT_PAGE_HPP
