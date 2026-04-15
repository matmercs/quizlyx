#include "ui/pages/QuizEditorPage.hpp"

#include <algorithm>

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

#include "model/Protocol.hpp"

namespace quizlyx::client::ui::pages {

namespace {

QString defaultHostName() {
  return QStringLiteral("Host");
}

QString questionSummary(const model::Question& question, int index) {
  const auto title = question.text.trimmed().isEmpty() ? QStringLiteral("Untitled question") : question.text.trimmed();
  const auto type = question.type == model::AnswerType::MultipleChoice ? QStringLiteral("multiple choice")
                                                                       : QStringLiteral("single choice");
  return QStringLiteral("%1. %2\n%3").arg(index + 1).arg(title, type);
}

} // namespace

QuizEditorPage::QuizEditorPage(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("quizEditorPage"));

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(16, 14, 16, 14);
  root->setSpacing(10);

  auto* title = new QLabel(QStringLiteral("Build a quiz"));
  title->setObjectName(QStringLiteral("titleLabel"));
  root->addWidget(title);

  auto* body = new QGridLayout;
  body->setHorizontalSpacing(12);
  body->setVerticalSpacing(10);
  body->setColumnStretch(1, 1);
  body->setRowStretch(1, 1);

  auto* metadataColumn = new QVBoxLayout;
  metadataColumn->setSpacing(8);
  auto* metadataTitle = new QLabel(QStringLiteral("Session details"));
  metadataTitle->setObjectName(QStringLiteral("subtitleLabel"));
  metadataColumn->addWidget(metadataTitle);

  auto* metadataForm = new QFormLayout;
  metadataForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  metadataForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  metadataForm->setHorizontalSpacing(10);
  metadataForm->setVerticalSpacing(8);

  titleEdit_ = new QLineEdit(QStringLiteral("My Quiz"));
  descriptionEdit_ = new QLineEdit(QStringLiteral("A Quizlyx quiz"));
  hostNameEdit_ = new QLineEdit(defaultHostName());
  hostSpectatorCheck_ = new QCheckBox(QStringLiteral("Host is spectator"));
  autoAdvanceEnabledCheck_ = new QCheckBox(QStringLiteral("Enable"));
  autoAdvanceSpin_ = new QSpinBox;
  autoAdvanceSpin_->setRange(1, 60);
  autoAdvanceSpin_->setSuffix(QStringLiteral(" s"));
  autoAdvanceSpin_->setValue(3);
  autoAdvanceSpin_->setMinimumWidth(100);
  autoAdvanceSpin_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  autoAdvanceSpin_->setEnabled(false);

  auto* hostModeRow = new QWidget;
  auto* hostModeLayout = new QHBoxLayout(hostModeRow);
  hostModeLayout->setContentsMargins(0, 0, 0, 0);
  hostModeLayout->setSpacing(0);
  hostModeLayout->addWidget(hostSpectatorCheck_, 0, Qt::AlignLeft | Qt::AlignVCenter);
  hostModeLayout->addStretch(1);

  auto* autoAdvanceRow = new QWidget;
  autoAdvanceRow->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  auto* autoAdvanceLayout = new QVBoxLayout(autoAdvanceRow);
  autoAdvanceLayout->setSizeConstraint(QLayout::SetMinimumSize);
  autoAdvanceLayout->setContentsMargins(0, 0, 0, 0);
  autoAdvanceLayout->setSpacing(metadataForm->verticalSpacing());
  autoAdvanceLayout->addWidget(autoAdvanceSpin_, 0, Qt::AlignLeft);
  autoAdvanceLayout->addWidget(autoAdvanceEnabledCheck_, 0, Qt::AlignLeft);
  autoAdvanceRow->setMinimumHeight(autoAdvanceSpin_->sizeHint().height() +
                                   autoAdvanceEnabledCheck_->sizeHint().height() + autoAdvanceLayout->spacing());

  metadataForm->addRow(QStringLiteral("Title"), titleEdit_);
  metadataForm->addRow(QStringLiteral("Description"), descriptionEdit_);
  metadataForm->addRow(QStringLiteral("Host name"), hostNameEdit_);
  metadataForm->addRow(QStringLiteral("Host mode"), hostModeRow);
  metadataForm->addRow(QStringLiteral("Auto-advance"), autoAdvanceRow);
  metadataColumn->addLayout(metadataForm);
  body->addLayout(metadataColumn, 0, 1);

  auto* listColumn = new QVBoxLayout;
  listColumn->setSpacing(8);
  auto* listTitle = new QLabel(QStringLiteral("Questions"));
  listTitle->setObjectName(QStringLiteral("subtitleLabel"));
  listColumn->addWidget(listTitle);

  questionList_ = new QListWidget;
  questionList_->setObjectName(QStringLiteral("questionList"));
  questionList_->setMinimumWidth(220);
  questionList_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  questionList_->setMinimumHeight(210);
  questionList_->setAlternatingRowColors(true);
  listColumn->addWidget(questionList_);

  auto* questionBtns = new QHBoxLayout;
  auto* addBtn = new QPushButton(QStringLiteral("Add"));
  removeQuestionBtn_ = new QPushButton(QStringLiteral("Remove"));
  moveUpBtn_ = new QPushButton(QStringLiteral("Move up"));
  moveDownBtn_ = new QPushButton(QStringLiteral("Move down"));
  questionBtns->addWidget(addBtn);
  questionBtns->addWidget(removeQuestionBtn_);
  questionBtns->addWidget(moveUpBtn_);
  questionBtns->addWidget(moveDownBtn_);
  listColumn->addLayout(questionBtns);
  listColumn->addStretch(1);
  body->addLayout(listColumn, 1, 0, Qt::AlignTop);

  auto* editorColumn = new QVBoxLayout;
  editorColumn->setSpacing(8);

  auto* editorTitle = new QLabel(QStringLiteral("Question details"));
  editorTitle->setObjectName(QStringLiteral("subtitleLabel"));
  editorColumn->addWidget(editorTitle);

  auto* questionForm = new QFormLayout;
  questionForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  questionForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  questionForm->setHorizontalSpacing(10);
  questionForm->setVerticalSpacing(8);

  questionTextEdit_ = new QTextEdit;
  questionTextEdit_->setPlaceholderText(QStringLiteral("Enter the question prompt"));
  questionTextEdit_->setMinimumHeight(64);
  questionTextEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);

  questionTypeCombo_ = new QComboBox;
  questionTypeCombo_->setObjectName(QStringLiteral("questionTypeCombo"));
  questionTypeCombo_->addItem(QStringLiteral("Single choice"), static_cast<int>(model::AnswerType::SingleChoice));
  questionTypeCombo_->addItem(QStringLiteral("Multiple choice"), static_cast<int>(model::AnswerType::MultipleChoice));
  questionTypeCombo_->setMaxVisibleItems(2);
  questionTypeCombo_->setMinimumContentsLength(16);
  questionTypeCombo_->setMinimumWidth(220);
  questionTypeCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  questionTypeCombo_->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
  auto* typeComboView = new QListView(questionTypeCombo_);
  typeComboView->setWordWrap(false);
  typeComboView->setTextElideMode(Qt::ElideNone);
  typeComboView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  typeComboView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  typeComboView->setUniformItemSizes(true);
  typeComboView->setMinimumWidth(220);
  typeComboView->setSpacing(0);
  questionTypeCombo_->setView(typeComboView);
  if (auto* view = questionTypeCombo_->view(); view != nullptr) {
    view->setTextElideMode(Qt::ElideNone);
    view->setMinimumWidth(220);
    int rowsHeight = 0;
    for (int i = 0; i < questionTypeCombo_->count(); ++i) {
      int rowHeight = view->sizeHintForRow(i);
      if (rowHeight <= 0) {
        rowHeight = questionTypeCombo_->fontMetrics().height() + 10;
      }
      rowsHeight += rowHeight;
    }
    const int popupHeight = rowsHeight + 2 * view->frameWidth();
    view->setMinimumHeight(popupHeight);
    view->setMaximumHeight(popupHeight);
  }

  questionTimeSpin_ = new QSpinBox;
  questionTimeSpin_->setRange(1, 600);
  questionTimeSpin_->setSuffix(QStringLiteral(" s"));
  questionTimeSpin_->setMinimumWidth(150);
  questionTimeSpin_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

  questionForm->addRow(QStringLiteral("Prompt"), questionTextEdit_);
  questionForm->addRow(QStringLiteral("Type"), questionTypeCombo_);
  questionForm->addRow(QStringLiteral("Time limit"), questionTimeSpin_);
  editorColumn->addLayout(questionForm);

  auto* optionsTitle = new QLabel(QStringLiteral("Options"));
  optionsTitle->setObjectName(QStringLiteral("subtitleLabel"));
  editorColumn->addWidget(optionsTitle);

  optionsContainer_ = new QWidget;
  optionsLayout_ = new QVBoxLayout(optionsContainer_);
  optionsLayout_->setContentsMargins(0, 0, 0, 0);
  optionsLayout_->setSpacing(6);
  editorColumn->addWidget(optionsContainer_);

  auto* addOptionBtn = new QPushButton(QStringLiteral("Add option"));
  editorColumn->addWidget(addOptionBtn, 0, Qt::AlignLeft);

  body->addLayout(editorColumn, 1, 1);
  root->addLayout(body);
  root->addStretch(1);

  statusLabel_ = new QLabel;
  statusLabel_->setObjectName(QStringLiteral("statusLabel"));

  createBtn_ = new QPushButton(QStringLiteral("Create session"));
  createBtn_->setObjectName(QStringLiteral("primaryButton"));

  auto* footer = new QHBoxLayout;
  auto* backBtn = new QPushButton(QStringLiteral("← Back"));
  auto* loadBtn = new QPushButton(QStringLiteral("Load JSON…"));
  auto* saveBtn = new QPushButton(QStringLiteral("Save JSON…"));
  footer->addWidget(backBtn);
  footer->addStretch(1);
  footer->addWidget(loadBtn);
  footer->addWidget(saveBtn);
  footer->addSpacing(8);
  footer->addWidget(statusLabel_, 1);
  footer->addWidget(createBtn_);
  root->addLayout(footer);

  connect(addBtn, &QPushButton::clicked, this, &QuizEditorPage::onAddClicked);
  connect(removeQuestionBtn_, &QPushButton::clicked, this, &QuizEditorPage::onRemoveClicked);
  connect(moveUpBtn_, &QPushButton::clicked, this, &QuizEditorPage::onMoveUpClicked);
  connect(moveDownBtn_, &QPushButton::clicked, this, &QuizEditorPage::onMoveDownClicked);
  connect(loadBtn, &QPushButton::clicked, this, &QuizEditorPage::onLoadClicked);
  connect(saveBtn, &QPushButton::clicked, this, &QuizEditorPage::onSaveClicked);
  connect(backBtn, &QPushButton::clicked, this, &QuizEditorPage::backRequested);
  connect(createBtn_, &QPushButton::clicked, this, &QuizEditorPage::onCreateClicked);
  connect(addOptionBtn, &QPushButton::clicked, this, &QuizEditorPage::onAddOptionClicked);
  connect(questionList_, &QListWidget::currentRowChanged, this, &QuizEditorPage::onQuestionSelectionChanged);
  connect(questionTextEdit_, &QTextEdit::textChanged, this, &QuizEditorPage::onCurrentQuestionEdited);
  connect(questionTimeSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { onCurrentQuestionEdited(); });
  connect(questionTypeCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { onTypeChanged(); });
  connect(autoAdvanceEnabledCheck_, &QCheckBox::toggled, this, [this](bool checked) {
    autoAdvanceSpin_->setEnabled(checked);
  });

  model::Quiz seed;
  seed.title = QStringLiteral("My Quiz");
  seed.description = QStringLiteral("A Quizlyx quiz");
  seed.questions.push_back(makeDefaultQuestion());
  setQuiz(seed);
}

model::Quiz QuizEditorPage::buildQuiz() const {
  model::Quiz quiz;
  quiz.title = titleEdit_->text().trimmed();
  quiz.description = descriptionEdit_->text().trimmed();
  quiz.questions = questions_;
  return quiz;
}

void QuizEditorPage::setQuiz(const model::Quiz& quiz) {
  syncingUi_ = true;
  titleEdit_->setText(quiz.title);
  descriptionEdit_->setText(quiz.description);
  questions_ = quiz.questions;
  if (questions_.isEmpty()) {
    questions_.push_back(makeDefaultQuestion());
  }
  currentQuestionIndex_ = 0;
  refreshQuestionList();
  questionList_->setCurrentRow(currentQuestionIndex_);
  loadQuestionIntoEditor(currentQuestionIndex_);
  syncingUi_ = false;
}

QString QuizEditorPage::hostName() const {
  const auto value = hostNameEdit_->text().trimmed();
  return value.isEmpty() ? defaultHostName() : value;
}

bool QuizEditorPage::hostIsSpectator() const {
  return hostSpectatorCheck_->isChecked();
}

int QuizEditorPage::autoAdvanceDelayMs() const {
  return autoAdvanceEnabledCheck_->isChecked() ? autoAdvanceSpin_->value() * 1000 : 0;
}

void QuizEditorPage::onAddClicked() {
  saveCurrentQuestion();
  questions_.push_back(makeDefaultQuestion());
  currentQuestionIndex_ = static_cast<int>(questions_.size()) - 1;
  refreshQuestionList();
  questionList_->setCurrentRow(currentQuestionIndex_);
  loadQuestionIntoEditor(currentQuestionIndex_);
  updateQuestionActionButtons();
}

void QuizEditorPage::onRemoveClicked() {
  if (currentQuestionIndex_ < 0 || currentQuestionIndex_ >= questions_.size()) {
    return;
  }

  if (questions_.size() == 1) {
    questions_.front() = makeDefaultQuestion();
    currentQuestionIndex_ = 0;
  } else {
    questions_.removeAt(currentQuestionIndex_);
    currentQuestionIndex_ = std::max(0, currentQuestionIndex_ - 1);
  }

  refreshQuestionList();
  questionList_->setCurrentRow(currentQuestionIndex_);
  loadQuestionIntoEditor(currentQuestionIndex_);
  updateQuestionActionButtons();
}

void QuizEditorPage::onMoveUpClicked() {
  if (currentQuestionIndex_ <= 0 || currentQuestionIndex_ >= questions_.size()) {
    return;
  }
  questions_.swapItemsAt(currentQuestionIndex_, currentQuestionIndex_ - 1);
  --currentQuestionIndex_;
  refreshQuestionList();
  questionList_->setCurrentRow(currentQuestionIndex_);
  updateQuestionActionButtons();
}

void QuizEditorPage::onMoveDownClicked() {
  if (currentQuestionIndex_ < 0 || currentQuestionIndex_ + 1 >= questions_.size()) {
    return;
  }
  questions_.swapItemsAt(currentQuestionIndex_, currentQuestionIndex_ + 1);
  ++currentQuestionIndex_;
  refreshQuestionList();
  questionList_->setCurrentRow(currentQuestionIndex_);
  updateQuestionActionButtons();
}

void QuizEditorPage::onLoadClicked() {
  const auto path =
      QFileDialog::getOpenFileName(this, QStringLiteral("Load quiz"), QString(), QStringLiteral("JSON (*.json)"));
  if (path.isEmpty())
    return;

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(this, QStringLiteral("Load failed"), file.errorString());
    return;
  }

  QJsonParseError error{};
  const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
  if (error.error != QJsonParseError::NoError) {
    QMessageBox::warning(this, QStringLiteral("Load failed"), error.errorString());
    return;
  }

  setQuiz(model::quizFromJson(doc.object()));
}

void QuizEditorPage::onSaveClicked() {
  saveCurrentQuestion();
  const auto path = QFileDialog::getSaveFileName(
      this, QStringLiteral("Save quiz"), QStringLiteral("quiz.json"), QStringLiteral("JSON (*.json)"));
  if (path.isEmpty())
    return;

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    QMessageBox::warning(this, QStringLiteral("Save failed"), file.errorString());
    return;
  }

  file.write(QJsonDocument(model::toJson(buildQuiz())).toJson(QJsonDocument::Indented));
}

void QuizEditorPage::onCreateClicked() {
  saveCurrentQuestion();
  const auto quiz = buildQuiz();
  if (!model::isValid(quiz)) {
    statusLabel_->setText(QStringLiteral(
        "Quiz is not valid — each question needs text, 2+ options, marked correct answers, and time > 0."));
    return;
  }

  statusLabel_->clear();
  emit hostRequested(quiz, hostName(), hostIsSpectator(), autoAdvanceDelayMs());
}

void QuizEditorPage::onQuestionSelectionChanged(int row) {
  if (syncingUi_ || row < 0 || row >= questions_.size())
    return;

  saveCurrentQuestion();
  currentQuestionIndex_ = row;
  loadQuestionIntoEditor(row);
  refreshQuestionList();
  updateQuestionActionButtons();
}

void QuizEditorPage::onCurrentQuestionEdited() {
  if (syncingUi_)
    return;

  saveCurrentQuestion();
  refreshQuestionList();
}

void QuizEditorPage::onTypeChanged() {
  if (syncingUi_ || currentQuestionIndex_ < 0 || currentQuestionIndex_ >= questions_.size())
    return;

  saveCurrentQuestion();
  auto& question = questions_[currentQuestionIndex_];
  question.type =
      questionTypeCombo_->currentIndex() == 1 ? model::AnswerType::MultipleChoice : model::AnswerType::SingleChoice;
  if (question.type == model::AnswerType::SingleChoice && question.correctIndices.size() > 1) {
    question.correctIndices = {question.correctIndices.first()};
  }
  loadQuestionIntoEditor(currentQuestionIndex_);
  refreshQuestionList();
}

void QuizEditorPage::onAddOptionClicked() {
  if (currentQuestionIndex_ < 0 || currentQuestionIndex_ >= questions_.size())
    return;

  saveCurrentQuestion();
  auto& question = questions_[currentQuestionIndex_];
  question.options.push_back(QStringLiteral("Option %1").arg(question.options.size() + 1));
  loadQuestionIntoEditor(currentQuestionIndex_);
}

void QuizEditorPage::loadQuestionIntoEditor(int index) {
  syncingUi_ = true;

  if (index < 0 || index >= questions_.size()) {
    questionTextEdit_->clear();
    questionTimeSpin_->setValue(15);
    syncingUi_ = false;
    return;
  }

  const auto& question = questions_.at(index);
  questionTextEdit_->setPlainText(question.text);
  questionTypeCombo_->setCurrentIndex(question.type == model::AnswerType::MultipleChoice ? 1 : 0);
  questionTimeSpin_->setValue(std::max(1, (question.timeLimitMs + 999) / 1000));
  rebuildOptionEditors();
  updateQuestionActionButtons();
  syncingUi_ = false;
}

void QuizEditorPage::saveCurrentQuestion() {
  if (syncingUi_ || currentQuestionIndex_ < 0 || currentQuestionIndex_ >= questions_.size())
    return;

  auto& question = questions_[currentQuestionIndex_];
  question.text = questionTextEdit_->toPlainText().trimmed();
  question.type =
      questionTypeCombo_->currentIndex() == 1 ? model::AnswerType::MultipleChoice : model::AnswerType::SingleChoice;
  question.timeLimitMs = questionTimeSpin_->value() * 1000;
  question.options.clear();
  question.correctIndices.clear();

  for (int index = 0; index < optionRows_.size(); ++index) {
    const auto& row = optionRows_.at(index);
    question.options.push_back(row.textEdit->text().trimmed());

    if (auto* radio = qobject_cast<QRadioButton*>(row.correctControl); radio != nullptr) {
      if (radio->isChecked()) {
        question.correctIndices = {index};
      }
    } else if (auto* check = qobject_cast<QCheckBox*>(row.correctControl); check != nullptr) {
      if (check->isChecked()) {
        question.correctIndices.push_back(index);
      }
    }
  }
}

void QuizEditorPage::refreshQuestionList() {
  QSignalBlocker blocker(questionList_);
  questionList_->clear();
  for (int index = 0; index < questions_.size(); ++index) {
    auto* item = new QListWidgetItem(questionSummary(questions_.at(index), index));
    item->setToolTip(questions_.at(index).text);
    questionList_->addItem(item);
  }

  if (!questions_.isEmpty()) {
    const int lastIndex = static_cast<int>(questions_.size()) - 1;
    currentQuestionIndex_ = std::clamp(currentQuestionIndex_, 0, lastIndex);
    questionList_->setCurrentRow(currentQuestionIndex_);
  }
  updateQuestionActionButtons();
}

void QuizEditorPage::rebuildOptionEditors() {
  while (auto* item = optionsLayout_->takeAt(0)) {
    if (auto* widget = item->widget()) {
      widget->deleteLater();
    }
    delete item;
  }
  optionRows_.clear();

  delete correctButtonGroup_;
  correctButtonGroup_ = nullptr;

  if (currentQuestionIndex_ < 0 || currentQuestionIndex_ >= questions_.size())
    return;

  auto& question = questions_[currentQuestionIndex_];
  if (question.options.size() < 2) {
    while (question.options.size() < 2) {
      question.options.push_back(QStringLiteral("Option %1").arg(question.options.size() + 1));
    }
  }

  if (question.type == model::AnswerType::SingleChoice) {
    correctButtonGroup_ = new QButtonGroup(this);
    correctButtonGroup_->setExclusive(true);
  }

  for (int index = 0; index < question.options.size(); ++index) {
    OptionRow row;
    row.container = new QWidget(optionsContainer_);
    auto* rowLayout = new QHBoxLayout(row.container);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(6);

    if (question.type == model::AnswerType::SingleChoice) {
      auto* radio = new QRadioButton(row.container);
      radio->setObjectName(QStringLiteral("quizOptionRadio"));
      radio->setChecked(question.correctIndices.contains(index));
      correctButtonGroup_->addButton(radio, index);
      connect(radio, &QRadioButton::toggled, this, [this](bool) { onCurrentQuestionEdited(); });
      row.correctControl = radio;
      rowLayout->addWidget(radio);
    } else {
      auto* check = new QCheckBox(row.container);
      check->setChecked(question.correctIndices.contains(index));
      connect(check, &QCheckBox::toggled, this, [this](bool) { onCurrentQuestionEdited(); });
      row.correctControl = check;
      rowLayout->addWidget(check);
    }

    row.textEdit = new QLineEdit(question.options.at(index), row.container);
    connect(row.textEdit, &QLineEdit::textChanged, this, [this](const QString&) { onCurrentQuestionEdited(); });
    rowLayout->addWidget(row.textEdit, 1);

    row.removeBtn = new QPushButton(QStringLiteral("Remove"), row.container);
    row.removeBtn->setEnabled(question.options.size() > 2);
    connect(row.removeBtn, &QPushButton::clicked, this, [this, index]() {
      if (currentQuestionIndex_ < 0 || currentQuestionIndex_ >= questions_.size())
        return;

      auto& current = questions_[currentQuestionIndex_];
      if (current.options.size() <= 2)
        return;

      current.options.removeAt(index);
      QVector<int> corrected;
      for (int correctIndex : current.correctIndices) {
        if (correctIndex == index)
          continue;
        corrected.push_back(correctIndex > index ? correctIndex - 1 : correctIndex);
      }
      current.correctIndices = corrected;
      if (current.type == model::AnswerType::SingleChoice && current.correctIndices.size() > 1) {
        current.correctIndices = {current.correctIndices.first()};
      }
      loadQuestionIntoEditor(currentQuestionIndex_);
      refreshQuestionList();
      updateQuestionActionButtons();
    });
    rowLayout->addWidget(row.removeBtn);

    optionsLayout_->addWidget(row.container);
    optionRows_.push_back(row);
  }
}

model::Question QuizEditorPage::makeDefaultQuestion() const {
  model::Question question;
  question.text = QStringLiteral("Question?");
  question.type = model::AnswerType::SingleChoice;
  question.options = {
      QStringLiteral("Option 1"), QStringLiteral("Option 2"), QStringLiteral("Option 3"), QStringLiteral("Option 4")};
  question.correctIndices = {0};
  question.timeLimitMs = 15000;
  return question;
}

void QuizEditorPage::updateQuestionActionButtons() {
  const bool hasSelection = currentQuestionIndex_ >= 0 && currentQuestionIndex_ < questions_.size();
  if (removeQuestionBtn_ != nullptr)
    removeQuestionBtn_->setEnabled(hasSelection);
  if (moveUpBtn_ != nullptr)
    moveUpBtn_->setEnabled(hasSelection && currentQuestionIndex_ > 0);
  if (moveDownBtn_ != nullptr)
    moveDownBtn_->setEnabled(hasSelection && currentQuestionIndex_ + 1 < questions_.size());
}

} // namespace quizlyx::client::ui::pages
