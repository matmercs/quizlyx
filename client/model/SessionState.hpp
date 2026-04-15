#ifndef QUIZLYX_CLIENT_MODEL_SESSION_STATE_HPP
#define QUIZLYX_CLIENT_MODEL_SESSION_STATE_HPP

#include <QList>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QVector>
#include <optional>

#include "model/PlayerEntry.hpp"
#include "model/Quiz.hpp"

namespace quizlyx::client::model {

enum class Phase {
  Disconnected,
  Connecting,
  Connected,
  Lobby,
  Question,
  Leaderboard,
  Finished,
  Reconnecting,
};

enum class Role { None, Host, Player };

struct LeaderboardRow {
  QString playerId;
  int score = 0;
};

struct AnswerRevealState {
  QVector<int> correctIndices;
  QVector<int> mySelectedIndices;
  QVector<int> optionPickCounts;
  int revealDurationMs = 0;
};

class SessionState : public QObject {
  Q_OBJECT
public:
  explicit SessionState(QObject* parent = nullptr);

  Phase phase() const {
    return phase_;
  }
  Role role() const {
    return role_;
  }
  const QString& sessionId() const {
    return sessionId_;
  }
  const QString& pin() const {
    return pin_;
  }
  const QString& myPlayerId() const {
    return myPlayerId_;
  }
  const QString& myDisplayName() const {
    return myDisplayName_;
  }
  bool isCompeting() const {
    return myIsCompeting_;
  }
  const QVector<PlayerEntry>& players() const {
    return players_;
  }
  const QVector<LeaderboardRow>& leaderboard() const {
    return leaderboard_;
  }
  int nextRoundDelayMs() const {
    return nextRoundDelayMs_;
  }
  bool isRevealActive() const {
    return revealState_.has_value();
  }
  const AnswerRevealState& revealState() const {
    return *revealState_;
  }

  const Quiz& localQuiz() const {
    return localQuiz_;
  }
  bool hasCurrentQuestion() const {
    return currentQuestion_.has_value();
  }
  const Question& currentQuestion() const {
    return *currentQuestion_;
  }
  int currentQuestionIndex() const {
    return currentQuestionIndex_;
  }
  int totalQuestions() const {
    return totalQuestions_;
  }
  int remainingMs() const {
    return remainingMs_;
  }
  int questionDurationMs() const {
    return questionDurationMs_;
  }
  bool hasAnswered() const {
    return hasAnswered_;
  }

  void reset();
  void clearSession();

  void setPhase(Phase phase);
  void setRole(Role role);
  void setConnectionInfo(QString host, quint16 port);
  void setSession(QString sessionId, QString pin);
  void setMyIdentity(QString playerId, QString displayName, bool isCompeting = true);
  void setLocalQuiz(Quiz quiz);
  void setPlayers(QVector<PlayerEntry> players);

  void addPlayer(PlayerEntry player);
  void removePlayer(const QString& playerId);
  void clearPlayers();

  void startQuestion(int index, int totalQuestions, int durationMs, Question question);
  void applyTimerUpdate(int remainingMs);
  void markQuestionTimedOut();
  void showAnswerReveal(AnswerRevealState revealState);
  void markAnswered();
  void resetAnsweredFlag();

  void setLeaderboard(QVector<LeaderboardRow> rows, int nextRoundDelayMs = 0);
  void setFinalLeaderboard(QVector<LeaderboardRow> rows);

  QString host() const {
    return host_;
  }
  quint16 port() const {
    return port_;
  }

signals:
  void phaseChanged(Phase phase);
  void playersChanged();
  void leaderboardChanged();
  void questionStarted(int questionIndex, int durationMs);
  void remainingMsChanged(int remainingMs);
  void questionTimedOut();
  void answerRevealStarted();
  void gameFinished();
  void myIdentityChanged();

private:
  Phase phase_ = Phase::Disconnected;
  Role role_ = Role::None;
  QString host_;
  quint16 port_ = 0;
  QString sessionId_;
  QString pin_;
  QString myPlayerId_;
  QString myDisplayName_;
  bool myIsCompeting_ = true;
  Quiz localQuiz_;
  QVector<PlayerEntry> players_;
  QVector<LeaderboardRow> leaderboard_;
  int nextRoundDelayMs_ = 0;
  std::optional<AnswerRevealState> revealState_;

  std::optional<Question> currentQuestion_;
  int currentQuestionIndex_ = -1;
  int totalQuestions_ = 0;
  int questionDurationMs_ = 0;
  int remainingMs_ = 0;
  bool hasAnswered_ = false;

  void syncMyCompetitionFromPlayers();
};

} // namespace quizlyx::client::model

Q_DECLARE_METATYPE(quizlyx::client::model::Phase)
Q_DECLARE_METATYPE(quizlyx::client::model::Role)
Q_DECLARE_METATYPE(quizlyx::client::model::LeaderboardRow)
Q_DECLARE_METATYPE(quizlyx::client::model::AnswerRevealState)

#endif // QUIZLYX_CLIENT_MODEL_SESSION_STATE_HPP
