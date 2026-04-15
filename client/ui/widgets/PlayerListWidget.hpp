#ifndef QUIZLYX_CLIENT_UI_WIDGETS_PLAYER_LIST_WIDGET_HPP
#define QUIZLYX_CLIENT_UI_WIDGETS_PLAYER_LIST_WIDGET_HPP

#include <QListWidget>
#include <QVector>

#include "model/PlayerEntry.hpp"

namespace quizlyx::client::ui::widgets {

class PlayerListWidget : public QListWidget {
  Q_OBJECT
public:
  explicit PlayerListWidget(QWidget* parent = nullptr);

  void setPlayers(const QVector<model::PlayerEntry>& players, const QString& myPlayerId);
};

} // namespace quizlyx::client::ui::widgets

#endif // QUIZLYX_CLIENT_UI_WIDGETS_PLAYER_LIST_WIDGET_HPP
