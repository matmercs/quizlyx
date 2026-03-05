#include <chrono>
#include <iostream>

#include "app/command_handler.hpp"
#include "domain/quiz.hpp"
#include "domain/types.hpp"
#include "interfaces/ibroadcast_sink.hpp"
#include "interfaces/itime_provider.hpp"
#include "services/in_memory_quiz_storage.hpp"
#include "services/quiz_registry.hpp"
#include "services/session_manager.hpp"
#include "services/session_timer_service.hpp"

namespace quizlyx::server {

class SteadyTimeProvider : public interfaces::ITimeProvider {
 public:
  std::chrono::steady_clock::time_point Now() const override {
    return std::chrono::steady_clock::now();
  }
};

class NoOpBroadcastSink : public interfaces::IBroadcastSink {
 public:
  void Broadcast(const std::string& /*session_id*/,
                 const ::quizlyx::server::events::GameEvent& /*event*/) override {}
};

}  // namespace quizlyx::server

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  using namespace quizlyx::server;

  services::InMemoryQuizStorage quiz_storage;
  services::QuizRegistry quiz_registry{quiz_storage};
  NoOpBroadcastSink broadcast_sink;
  SteadyTimeProvider time_provider;
  services::SessionManager session_manager{quiz_registry, broadcast_sink};
  services::SessionTimerService timer_service{time_provider,
                                              std::chrono::milliseconds{100}};

  app::ServerCommandHandler commands{quiz_registry, session_manager};

  // Демо бизнес-логики

  domain::Quiz quiz;
  quiz.title = "Demo Quiz";
  quiz.description = "One question";
  quiz.questions.push_back(
      {"What is 2+2?", domain::AnswerType::SingleChoice, {"3", "4", "5"}, {1},
       std::chrono::milliseconds(5000)});

  auto quiz_code = commands.CreateQuiz(std::move(quiz));
  if (!quiz_code) {
    std::cerr << "CreateQuiz failed\n";
    return 1;
  }
  std::cout << "Quiz created: code=" << *quiz_code << "\n";

  auto created = commands.CreateSession(*quiz_code, "host1");
  if (!created) {
    std::cerr << "CreateSession failed\n";
    return 1;
  }
  std::cout << "Session created: id=" << created->session_id << " pin=" << created->pin << "\n";

  if (!commands.JoinAsPlayer(created->pin, "player1")) {
    std::cerr << "JoinAsPlayer failed\n";
    return 1;
  }
  std::cout << "Player joined\n";

  if (!commands.StartGame(created->session_id)) {
    std::cerr << "StartGame failed\n";
    return 1;
  }
  std::cout << "Game started\n";

  domain::PlayerAnswer answer;
  answer.selected_indices = {1};
  answer.time_since_question_start_ms = std::chrono::milliseconds(100);
  if (!commands.SubmitAnswer(created->session_id, "player1", answer)) {
    std::cerr << "SubmitAnswer failed\n";
    return 1;
  }
  std::cout << "Answer submitted\n";

  if (!commands.NextQuestion(created->session_id)) {
    std::cerr << "NextQuestion failed\n";
    return 1;
  }
  std::cout << "NextQuestion (game finished)\n";

  auto session = session_manager.GetSessionById(created->session_id);
  if (!session) {
    std::cerr << "GetSessionById failed\n";
    return 1;
  }
  std::cout << "Session state: "
            << (session->state == domain::SessionState::Finished ? "Finished" : "?")
            << ", players: " << session->players.size() << "\n";
  for (const auto& p : session->players) {
    std::cout << "  " << p.id << " ("
              << (p.role == domain::Role::Host ? "Host" : "Player") << ") score=" << p.score
              << "\n";
  }

  std::cout << "Demo OK.\n";
  return 0;
}

