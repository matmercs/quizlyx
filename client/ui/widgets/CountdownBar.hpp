#ifndef QUIZLYX_CLIENT_UI_WIDGETS_COUNTDOWN_BAR_HPP
#define QUIZLYX_CLIENT_UI_WIDGETS_COUNTDOWN_BAR_HPP

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

namespace quizlyx::client::ui::widgets {

class CountdownBar : public QWidget {
  Q_OBJECT
public:
  explicit CountdownBar(QWidget* parent = nullptr);

  void setDuration(int durationMs);
  void setRemainingMs(int remainingMs);
  void stop();

protected:
  void paintEvent(QPaintEvent* event) override;

private slots:
  void onTick();

private:
  int durationMs_ = 0;
  int lastServerRemainingMs_ = 0;
  QElapsedTimer sinceLastServerUpdate_;
  QTimer tick_;
  bool active_ = false;
};

} // namespace quizlyx::client::ui::widgets

#endif // QUIZLYX_CLIENT_UI_WIDGETS_COUNTDOWN_BAR_HPP
