#ifndef QUIZLYX_CLIENT_UI_PAGES_QUIZ_EDITOR_PAGE_HPP
#define QUIZLYX_CLIENT_UI_PAGES_QUIZ_EDITOR_PAGE_HPP

#include <QWidget>

#include "model/Quiz.hpp"

class QLabel;
class QListWidget;
class QComboBox;
class QButtonGroup;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTextEdit;
class QVBoxLayout;
class QWidget;
class QCheckBox;

namespace quizlyx::client::ui::pages {

class QuizEditorPage : public QWidget {
  Q_OBJECT
public:
  explicit QuizEditorPage(QWidget* parent = nullptr);

  model::Quiz buildQuiz() const;
  void setQuiz(const model::Quiz& quiz);

  QString hostName() const;
  int autoAdvanceDelayMs() const;
  bool hostIsSpectator() const;

signals:
  void hostRequested(model::Quiz quiz, QString hostName, bool hostIsSpectator, int autoAdvanceDelayMs);
  void backRequested();

private slots:
  void onAddClicked();
  void onRemoveClicked();
  void onMoveUpClicked();
  void onMoveDownClicked();
  void onLoadClicked();
  void onSaveClicked();
  void onCreateClicked();
  void onQuestionSelectionChanged(int row);
  void onCurrentQuestionEdited();
  void onTypeChanged();
  void onAddOptionClicked();

private:
  struct OptionRow {
    QWidget* container = nullptr;
    QWidget* correctControl = nullptr;
    QLineEdit* textEdit = nullptr;
    QPushButton* removeBtn = nullptr;
  };

  void loadQuestionIntoEditor(int index);
  void saveCurrentQuestion();
  void refreshQuestionList();
  void rebuildOptionEditors();
  void updateQuestionActionButtons();
  model::Question makeDefaultQuestion() const;

  QListWidget* questionList_ = nullptr;
  QLineEdit* titleEdit_ = nullptr;
  QLineEdit* descriptionEdit_ = nullptr;
  QLineEdit* hostNameEdit_ = nullptr;
  QCheckBox* hostSpectatorCheck_ = nullptr;
  QCheckBox* autoAdvanceEnabledCheck_ = nullptr;
  QSpinBox* autoAdvanceSpin_ = nullptr;
  QTextEdit* questionTextEdit_ = nullptr;
  QComboBox* questionTypeCombo_ = nullptr;
  QSpinBox* questionTimeSpin_ = nullptr;
  QWidget* optionsContainer_ = nullptr;
  QVBoxLayout* optionsLayout_ = nullptr;
  QPushButton* removeQuestionBtn_ = nullptr;
  QPushButton* moveUpBtn_ = nullptr;
  QPushButton* moveDownBtn_ = nullptr;
  QPushButton* createBtn_ = nullptr;
  QLabel* statusLabel_ = nullptr;
  QVector<OptionRow> optionRows_;
  QButtonGroup* correctButtonGroup_ = nullptr;
  QVector<model::Question> questions_;
  int currentQuestionIndex_ = -1;
  bool syncingUi_ = false;
};

} // namespace quizlyx::client::ui::pages

#endif // QUIZLYX_CLIENT_UI_PAGES_QUIZ_EDITOR_PAGE_HPP
