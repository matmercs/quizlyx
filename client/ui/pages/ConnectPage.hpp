#ifndef QUIZLYX_CLIENT_UI_PAGES_CONNECT_PAGE_HPP
#define QUIZLYX_CLIENT_UI_PAGES_CONNECT_PAGE_HPP

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

namespace quizlyx::client::ui::pages {

class ConnectPage : public QWidget {
  Q_OBJECT
public:
  explicit ConnectPage(QWidget* parent = nullptr);

  void setStatus(const QString& text);
  void setBusy(bool busy);
  void resetUiState();

signals:
  void connectRequested(QString host, quint16 port);

private slots:
  void onConnectClicked();

private:
  QLineEdit* hostEdit_ = nullptr;
  QLineEdit* portEdit_ = nullptr;
  QPushButton* connectBtn_ = nullptr;
  QLabel* statusLabel_ = nullptr;
};

} // namespace quizlyx::client::ui::pages

#endif // QUIZLYX_CLIENT_UI_PAGES_CONNECT_PAGE_HPP
