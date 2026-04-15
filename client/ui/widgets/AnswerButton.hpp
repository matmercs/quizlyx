#ifndef QUIZLYX_CLIENT_UI_WIDGETS_ANSWER_BUTTON_HPP
#define QUIZLYX_CLIENT_UI_WIDGETS_ANSWER_BUTTON_HPP

#include <QLabel>
#include <QPushButton>
#include <QString>

namespace quizlyx::client::ui::widgets {

class AnswerButton : public QPushButton {
  Q_OBJECT
public:
  enum class RevealState { None, Correct, WrongSelected };

  AnswerButton(int index, QString text, QWidget* parent = nullptr);

  int index() const {
    return index_;
  }
  void setChecked(bool checked);
  bool isSelected() const {
    return selected_;
  }

  void setRevealState(RevealState revealState);
  void setPickCount(int pickCount);
  void showPickCount(bool show);
  void setLocked(bool locked);

signals:
  void answerClicked(int index);

private:
  void updateContent();
  void applyStyle();

  int index_;
  QString text_;
  bool selected_ = false;
  bool locked_ = false;
  bool showPickCount_ = false;
  int pickCount_ = 0;
  RevealState revealState_ = RevealState::None;
  QLabel* markerLabel_ = nullptr;
  QLabel* optionLabel_ = nullptr;
  QWidget* secondaryGap_ = nullptr;
  QLabel* countLabel_ = nullptr;
  QLabel* revealPrimaryLabel_ = nullptr;
  QLabel* revealSecondaryLabel_ = nullptr;
};

} // namespace quizlyx::client::ui::widgets

#endif // QUIZLYX_CLIENT_UI_WIDGETS_ANSWER_BUTTON_HPP
