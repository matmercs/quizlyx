#ifndef QUIZLYX_CLIENT_UI_WIDGETS_LEADERBOARD_WIDGET_HPP
#define QUIZLYX_CLIENT_UI_WIDGETS_LEADERBOARD_WIDGET_HPP

#include <QHash>
#include <QTableWidget>
#include <QVector>

#include "model/SessionState.hpp"

namespace quizlyx::client::ui::widgets {

class LeaderboardWidget : public QTableWidget {
  Q_OBJECT
public:
  explicit LeaderboardWidget(QWidget* parent = nullptr);

  void setRows(const QVector<model::LeaderboardRow>& rows,
               const QHash<QString, QString>& displayNameById,
               const QString& myPlayerId);
};

} // namespace quizlyx::client::ui::widgets

#endif // QUIZLYX_CLIENT_UI_WIDGETS_LEADERBOARD_WIDGET_HPP
