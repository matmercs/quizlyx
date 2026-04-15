#include "model/SessionState.hpp"

#include <algorithm>

namespace quizlyx::client::model {

SessionState::SessionState(QObject* parent) : QObject(parent) {
}

void SessionState::reset() {
  const bool phaseChanging = phase_ != Phase::Disconnected;
  host_.clear();
  port_ = 0;
  role_ = Role::None;
  sessionId_.clear();
  pin_.clear();
  myPlayerId_.clear();
  myDisplayName_.clear();
  myIsCompeting_ = true;
  localQuiz_ = Quiz{};
  players_.clear();
  leaderboard_.clear();
  nextRoundDelayMs_ = 0;
  revealState_.reset();
  currentQuestion_.reset();
  currentQuestionIndex_ = -1;
  totalQuestions_ = 0;
  questionDurationMs_ = 0;
  remainingMs_ = 0;
  hasAnswered_ = false;
  phase_ = Phase::Disconnected;
  emit playersChanged();
  emit leaderboardChanged();
  if (phaseChanging)
    emit phaseChanged(phase_);
  emit myIdentityChanged();
}

void SessionState::clearSession() {
  const bool phaseChanging = phase_ != Phase::Connected;
  role_ = Role::None;
  sessionId_.clear();
  pin_.clear();
  myPlayerId_.clear();
  myDisplayName_.clear();
  myIsCompeting_ = true;
  localQuiz_ = Quiz{};
  players_.clear();
  leaderboard_.clear();
  nextRoundDelayMs_ = 0;
  revealState_.reset();
  currentQuestion_.reset();
  currentQuestionIndex_ = -1;
  totalQuestions_ = 0;
  questionDurationMs_ = 0;
  remainingMs_ = 0;
  hasAnswered_ = false;
  emit playersChanged();
  emit leaderboardChanged();
  if (phaseChanging) {
    phase_ = Phase::Connected;
    emit phaseChanged(phase_);
  }
  emit myIdentityChanged();
}

void SessionState::setPhase(Phase phase) {
  if (phase_ == phase)
    return;
  phase_ = phase;
  emit phaseChanged(phase_);
}

void SessionState::setRole(Role role) {
  role_ = role;
}

void SessionState::setConnectionInfo(QString host, quint16 port) {
  host_ = std::move(host);
  port_ = port;
}

void SessionState::setSession(QString sessionId, QString pin) {
  sessionId_ = std::move(sessionId);
  pin_ = std::move(pin);
}

void SessionState::setMyIdentity(QString playerId, QString displayName, bool isCompeting) {
  myPlayerId_ = std::move(playerId);
  myDisplayName_ = std::move(displayName);
  myIsCompeting_ = isCompeting;
  emit myIdentityChanged();
}

void SessionState::setLocalQuiz(Quiz quiz) {
  localQuiz_ = std::move(quiz);
  totalQuestions_ = static_cast<int>(localQuiz_.questions.size());
}

void SessionState::setPlayers(QVector<PlayerEntry> players) {
  players_ = std::move(players);
  syncMyCompetitionFromPlayers();
  emit playersChanged();
}

void SessionState::addPlayer(PlayerEntry player) {
  auto it = std::find_if(players_.begin(), players_.end(), [&](const PlayerEntry& p) { return p.id == player.id; });
  if (it != players_.end()) {
    *it = std::move(player);
  } else {
    players_.push_back(std::move(player));
  }
  syncMyCompetitionFromPlayers();
  emit playersChanged();
}

void SessionState::removePlayer(const QString& playerId) {
  auto newEnd =
      std::remove_if(players_.begin(), players_.end(), [&](const PlayerEntry& p) { return p.id == playerId; });
  if (newEnd != players_.end()) {
    players_.erase(newEnd, players_.end());
    syncMyCompetitionFromPlayers();
    emit playersChanged();
  }
}

void SessionState::clearPlayers() {
  if (players_.isEmpty())
    return;
  players_.clear();
  syncMyCompetitionFromPlayers();
  emit playersChanged();
}

void SessionState::startQuestion(int index, int totalQuestions, int durationMs, Question question) {
  currentQuestionIndex_ = index;
  totalQuestions_ = totalQuestions;
  questionDurationMs_ = durationMs;
  remainingMs_ = durationMs;
  currentQuestion_ = std::move(question);
  hasAnswered_ = false;
  nextRoundDelayMs_ = 0;
  revealState_.reset();
  setPhase(Phase::Question);
  emit questionStarted(index, durationMs);
  emit remainingMsChanged(remainingMs_);
}

void SessionState::applyTimerUpdate(int remainingMs) {
  remainingMs_ = remainingMs;
  emit remainingMsChanged(remainingMs_);
}

void SessionState::markQuestionTimedOut() {
  remainingMs_ = 0;
  emit remainingMsChanged(remainingMs_);
  emit questionTimedOut();
}

void SessionState::showAnswerReveal(AnswerRevealState revealState) {
  revealState_ = std::move(revealState);
  emit answerRevealStarted();
}

void SessionState::markAnswered() {
  hasAnswered_ = true;
}

void SessionState::resetAnsweredFlag() {
  hasAnswered_ = false;
}

void SessionState::setLeaderboard(QVector<LeaderboardRow> rows, int nextRoundDelayMs) {
  leaderboard_ = std::move(rows);
  nextRoundDelayMs_ = nextRoundDelayMs;
  revealState_.reset();
  for (auto& p : players_) {
    for (const auto& row : leaderboard_) {
      if (row.playerId == p.id) {
        p.score = row.score;
        break;
      }
    }
  }
  emit leaderboardChanged();
  emit playersChanged();
}

void SessionState::setFinalLeaderboard(QVector<LeaderboardRow> rows) {
  setLeaderboard(std::move(rows));
  setPhase(Phase::Finished);
  emit gameFinished();
}

void SessionState::syncMyCompetitionFromPlayers() {
  if (myPlayerId_.isEmpty())
    return;

  auto it = std::find_if(
      players_.cbegin(), players_.cend(), [this](const PlayerEntry& player) { return player.id == myPlayerId_; });
  if (it != players_.cend()) {
    myIsCompeting_ = it->isCompeting;
  }
}

} // namespace quizlyx::client::model
