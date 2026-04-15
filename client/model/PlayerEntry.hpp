#ifndef QUIZLYX_CLIENT_MODEL_PLAYER_ENTRY_HPP
#define QUIZLYX_CLIENT_MODEL_PLAYER_ENTRY_HPP

#include <QMetaType>
#include <QString>

namespace quizlyx::client::model {

struct PlayerEntry {
  QString id;
  QString name;
  int score = 0;
  bool isHost = false;
  bool isCompeting = true;
  bool connected = true;
};

} // namespace quizlyx::client::model

Q_DECLARE_METATYPE(quizlyx::client::model::PlayerEntry)

#endif // QUIZLYX_CLIENT_MODEL_PLAYER_ENTRY_HPP
