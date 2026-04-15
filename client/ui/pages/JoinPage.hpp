#ifndef QUIZLYX_CLIENT_UI_PAGES_JOIN_PAGE_HPP
#define QUIZLYX_CLIENT_UI_PAGES_JOIN_PAGE_HPP

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

namespace quizlyx::client::ui::pages {

class JoinPage : public QWidget {
  Q_OBJECT
public:
  explicit JoinPage(QWidget* parent = nullptr);

  void setStatus(const QString& text);
  void setBusy(bool busy);
  void resetUiState();

signals:
  void joinRequested(QString pin, QString name);
  void backRequested();

private slots:
  void onJoinClicked();

private:
  QLineEdit* pinEdit_ = nullptr;
  QLineEdit* nameEdit_ = nullptr;
  QPushButton* joinBtn_ = nullptr;
  QLabel* statusLabel_ = nullptr;
};

} // namespace quizlyx::client::ui::pages

#endif // QUIZLYX_CLIENT_UI_PAGES_JOIN_PAGE_HPP
