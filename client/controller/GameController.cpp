#include "controller/GameController.hpp"

#include <QJsonArray>
#include <QUuid>

#include "network/NetworkClient.hpp"

namespace quizlyx::client::controller {

using model::Phase;
using model::Role;

namespace {

QVector<model::PlayerEntry> parsePlayers(const QJsonArray& players) {
  QVector<model::PlayerEntry> result;
  result.reserve(players.size());
  for (const auto& value : players) {
    const auto object = value.toObject();
    model::PlayerEntry entry;
    entry.id = object.value(QStringLiteral("player_id")).toString();
    entry.name = object.value(QStringLiteral("display_name")).toString();
    entry.isHost = object.value(QStringLiteral("role")).toString() == QStringLiteral("host");
    entry.isCompeting = object.value(QStringLiteral("is_competing")).toBool(true);
    entry.connected = object.value(QStringLiteral("connected")).toBool(true);
    result.push_back(std::move(entry));
  }
  return result;
}

} // namespace

GameController::GameController(network::NetworkClient* net, model::SessionState* state, QObject* parent) :
    QObject(parent), net_(net), state_(state) {
  connect(this, &GameController::requestConnect, net_, &network::NetworkClient::connectTo);
  connect(this, &GameController::requestDisconnect, net_, &network::NetworkClient::disconnectFromServer);
  connect(this, &GameController::requestSend, net_, &network::NetworkClient::sendRequest);

  connect(net_, &network::NetworkClient::connected, this, &GameController::onConnected);
  connect(net_, &network::NetworkClient::disconnected, this, &GameController::onDisconnected);
  connect(net_, &network::NetworkClient::errorOccurred, this, &GameController::onNetworkError);
  connect(net_, &network::NetworkClient::responseReceived, this, &GameController::onResponseReceived);
  connect(net_, &network::NetworkClient::questionStarted, this, &GameController::onQuestionStarted);
  connect(net_, &network::NetworkClient::timerUpdate, this, &GameController::onTimerUpdate);
  connect(net_, &network::NetworkClient::questionTimedOut, this, &GameController::onQuestionTimedOut);
  connect(net_, &network::NetworkClient::answerReveal, this, &GameController::onAnswerReveal);
  connect(net_, &network::NetworkClient::leaderboardUpdated, this, &GameController::onLeaderboardUpdated);
  connect(net_, &network::NetworkClient::playerJoined, this, &GameController::onPlayerJoined);
  connect(net_, &network::NetworkClient::playerLeft, this, &GameController::onPlayerLeft);
  connect(net_, &network::NetworkClient::gameFinished, this, &GameController::onGameFinished);
}

void GameController::connectToServer(QString host, quint16 port) {
  state_->setConnectionInfo(host, port);
  state_->setPhase(Phase::Connecting);
  emit requestConnect(std::move(host), port);
}

void GameController::disconnectFromServer() {
  emit requestDisconnect();
}

void GameController::hostGame(model::Quiz quiz, QString hostName, bool hostIsSpectator, int autoAdvanceDelayMs) {
  stagedQuiz_ = quiz;
  stagedHostId_ = hostName;
  stagedHostIsSpectator_ = hostIsSpectator;
  stagedAutoAdvanceMs_ = autoAdvanceDelayMs;
  state_->setRole(Role::Host);
  state_->setLocalQuiz(quiz);

  sendRequest(QStringLiteral("create_quiz"), model::toJson(quiz), [this](bool success, const QJsonObject& payload) {
    if (!success) {
      emit errorOccurred(payload.value(QStringLiteral("error")).toString(QStringLiteral("failed to create quiz")));
      return;
    }
    const auto quizCode = payload.value(QStringLiteral("quiz_code")).toString();
    QJsonObject sessionPayload{
        {QStringLiteral("quiz_code"), quizCode},
        {QStringLiteral("host_name"), stagedHostId_},
        {QStringLiteral("host_is_spectator"), stagedHostIsSpectator_},
    };
    if (stagedAutoAdvanceMs_ > 0)
      sessionPayload.insert(QStringLiteral("auto_advance_delay_ms"), stagedAutoAdvanceMs_);

    sendRequest(QStringLiteral("create_session"), sessionPayload, [this](bool ok, const QJsonObject& p) {
      if (!ok) {
        emit errorOccurred(p.value(QStringLiteral("error")).toString(QStringLiteral("failed to create session")));
        return;
      }
      const auto sid = p.value(QStringLiteral("session_id")).toString();
      const auto pin = p.value(QStringLiteral("pin")).toString();
      const auto pid = p.value(QStringLiteral("player_id")).toString();
      const auto displayName = p.value(QStringLiteral("display_name")).toString(stagedHostId_);
      const bool isCompeting = p.value(QStringLiteral("is_competing")).toBool(true);
      state_->setSession(sid, pin);
      state_->setPlayers(parsePlayers(p.value(QStringLiteral("players")).toArray()));
      state_->setMyIdentity(pid, displayName, isCompeting);
      state_->setPhase(Phase::Lobby);
      emit sessionCreated(sid, pin);
    });
  });
}

void GameController::joinGame(QString pin, QString name) {
  state_->setRole(Role::Player);
  stagedJoinName_ = name;
  QJsonObject payload{
      {QStringLiteral("pin"), pin},
      {QStringLiteral("name"), name},
  };
  sendRequest(QStringLiteral("join"), payload, [this, pin, name](bool success, const QJsonObject& p) {
    if (!success) {
      emit errorOccurred(p.value(QStringLiteral("error")).toString(QStringLiteral("failed to join")));
      return;
    }
    const auto sessionId = p.value(QStringLiteral("session_id")).toString();
    const auto pid = p.value(QStringLiteral("player_id")).toString();
    const auto displayName = p.value(QStringLiteral("display_name")).toString(name);
    const bool isCompeting = p.value(QStringLiteral("is_competing")).toBool(true);
    state_->setSession(sessionId, pin);
    state_->setPlayers(parsePlayers(p.value(QStringLiteral("players")).toArray()));
    state_->setMyIdentity(pid, displayName, isCompeting);
    state_->setPhase(Phase::Lobby);
    emit joinedSession(sessionId, pid);
  });
}

void GameController::startGame() {
  if (state_->role() != Role::Host)
    return;
  QJsonObject payload{{QStringLiteral("session_id"), state_->sessionId()}};
  sendRequest(QStringLiteral("start_game"), payload, [this](bool ok, const QJsonObject& p) {
    if (!ok) {
      emit errorOccurred(p.value(QStringLiteral("error")).toString(QStringLiteral("failed to start")));
    }
  });
}

void GameController::submitAnswer(QVector<int> selected) {
  if (state_->phase() != Phase::Question)
    return;
  if (!state_->isCompeting())
    return;
  if (state_->hasAnswered())
    return;

  const int timeMs = questionElapsedValid_ ? static_cast<int>(questionElapsed_.elapsed()) : 0;

  QJsonArray indices;
  for (int idx : selected)
    indices.push_back(idx);

  QJsonObject payload{
      {QStringLiteral("session_id"), state_->sessionId()},
      {QStringLiteral("player_id"), state_->myPlayerId()},
      {QStringLiteral("selected_indices"), indices},
      {QStringLiteral("time_ms"), timeMs},
  };

  state_->markAnswered();
  sendRequest(QStringLiteral("submit_answer"), payload, [this](bool ok, const QJsonObject& p) {
    if (!ok) {
      state_->resetAnsweredFlag();
      emit errorOccurred(p.value(QStringLiteral("error")).toString(QStringLiteral("failed to submit")));
    }
  });
}

void GameController::requestNextQuestion() {
  if (state_->role() != Role::Host)
    return;
  QJsonObject payload{{QStringLiteral("session_id"), state_->sessionId()}};
  sendRequest(QStringLiteral("next_question"), payload, [this](bool ok, const QJsonObject& p) {
    if (!ok) {
      emit errorOccurred(p.value(QStringLiteral("error")).toString(QStringLiteral("failed to advance")));
    }
  });
}

void GameController::leaveSession() {
  if (state_->sessionId().isEmpty() || state_->myPlayerId().isEmpty()) {
    state_->clearSession();
    return;
  }
  QJsonObject payload{
      {QStringLiteral("session_id"), state_->sessionId()},
      {QStringLiteral("player_id"), state_->myPlayerId()},
  };
  sendRequest(QStringLiteral("leave"), payload, [this](bool ok, const QJsonObject& p) {
    if (!ok) {
      emit errorOccurred(p.value(QStringLiteral("error")).toString(QStringLiteral("failed to leave")));
    }
    state_->clearSession();
  });
}

void GameController::onConnected() {
  state_->setPhase(Phase::Connected);
}

void GameController::onDisconnected() {
  if (state_->phase() != Phase::Disconnected) {
    state_->setPhase(Phase::Disconnected);
  }
}

void GameController::onNetworkError(QString message) {
  emit errorOccurred(std::move(message));
}

void GameController::onResponseReceived(QString id, bool success, QJsonObject payload) {
  auto it = pending_.find(id);
  if (it == pending_.end())
    return;
  auto handler = it.value();
  pending_.erase(it);
  handler(success, payload);
}

void GameController::onQuestionStarted(model::QuestionStartedEv ev) {
  questionElapsed_.restart();
  questionElapsedValid_ = true;
  state_->startQuestion(ev.questionIndex, ev.totalQuestions, ev.durationMs, ev.question);
}

void GameController::onTimerUpdate(model::TimerUpdateEv ev) {
  state_->applyTimerUpdate(ev.remainingMs);
}

void GameController::onQuestionTimedOut() {
  state_->markQuestionTimedOut();
  questionElapsedValid_ = false;
}

void GameController::onAnswerReveal(model::AnswerRevealEv ev) {
  questionElapsedValid_ = false;
  model::AnswerRevealState revealState;
  revealState.correctIndices = std::move(ev.correctIndices);
  revealState.mySelectedIndices = std::move(ev.mySelectedIndices);
  revealState.optionPickCounts = std::move(ev.optionPickCounts);
  revealState.revealDurationMs = ev.revealDurationMs;
  state_->showAnswerReveal(std::move(revealState));
}

void GameController::onLeaderboardUpdated(model::LeaderboardEv ev) {
  questionElapsedValid_ = false;
  state_->setLeaderboard(ev.entries, ev.nextRoundDelayMs);
  state_->setPhase(Phase::Leaderboard);
}

void GameController::onPlayerJoined(model::PlayerJoinedEv ev) {
  model::PlayerEntry entry;
  entry.id = ev.playerId;
  entry.name = ev.displayName;
  entry.isHost = (ev.role == QStringLiteral("host"));
  entry.isCompeting = ev.isCompeting;
  state_->addPlayer(std::move(entry));
}

void GameController::onPlayerLeft(model::PlayerLeftEv ev) {
  state_->removePlayer(ev.playerId);
}

void GameController::onGameFinished(model::GameFinishedEv ev) {
  questionElapsedValid_ = false;
  state_->setFinalLeaderboard(ev.finalLeaderboard);
}

QString GameController::nextRequestId() {
  return QString::number(nextId_++);
}

void GameController::sendRequest(const QString& type, const QJsonObject& payload, ResponseHandler handler) {
  const auto id = nextRequestId();
  pending_.insert(id, handler);
  emit requestSend(id, type, payload);
}

} // namespace quizlyx::client::controller
