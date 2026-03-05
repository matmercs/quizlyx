#include <chrono>
#include <iostream>
#include <string>

#include <gtest/gtest.h>

#include "app/command_handler.hpp"
#include "domain/quiz.hpp"
#include "domain/session.hpp"
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
                 const events::GameEvent& /*event*/) override {}
};

static int ScoreOf(const domain::Session& s, const std::string& player_id) {
  for (const auto& p : s.players)
    if (p.id == player_id) return p.score;
  return -1;
}

static const char* StateStr(domain::SessionState s) {
  switch (s) {
    case domain::SessionState::Lobby: return "Lobby";
    case domain::SessionState::Running: return "Running";
    case domain::SessionState::Finished: return "Finished";
  }
  return "?";
}

static const char* RoleStr(domain::Role r) {
  return r == domain::Role::Host ? "Host" : "Player";
}

static void PrintSession(const domain::Session& session, const std::string& title) {
  std::cout << "\n--- " << title << " ---\n";
  std::cout << "  state=" << StateStr(session.state)
            << "  question=" << session.current_question_index
            << "  has_deadline=" << (session.has_question_deadline ? "yes" : "no") << "\n";
  std::cout << "  players:\n";
  for (const auto& p : session.players) {
    std::cout << "    " << p.id << " [" << RoleStr(p.role) << "] score=" << p.score
              << "  answered_current=" << (p.answered_current_question ? "yes" : "no") << "\n";
  }
  std::cout << "---\n";
}

TEST(ServerBusinessLogicDemo, FullScenario) {
  services::InMemoryQuizStorage quiz_storage;
  services::QuizRegistry quiz_registry{quiz_storage};
  NoOpBroadcastSink broadcast_sink;
  SteadyTimeProvider time_provider;
  services::SessionManager session_manager{quiz_registry, broadcast_sink};
  services::SessionTimerService timer_service{time_provider,
                                              std::chrono::milliseconds{100}};
  app::ServerCommandHandler commands{quiz_registry, session_manager};

  domain::Quiz quiz;
  quiz.title = "Math & Logic";
  quiz.description = "Two questions";
  quiz.questions.push_back(
      {"What is 2+2?", domain::AnswerType::SingleChoice, {"3", "4", "5"}, {1},
       std::chrono::milliseconds(5000)});
  quiz.questions.push_back(
      {"Select even numbers", domain::AnswerType::MultipleChoice, {"2", "3", "4", "5"}, {0, 2},
       std::chrono::milliseconds(8000)});

  auto quiz_code = commands.CreateQuiz(std::move(quiz));
  ASSERT_TRUE(quiz_code.has_value()) << "CreateQuiz";
  EXPECT_FALSE(quiz_code->empty());

  auto created = commands.CreateSession(*quiz_code, "host1");
  ASSERT_TRUE(created.has_value()) << "CreateSession";
  EXPECT_FALSE(created->session_id.empty());
  EXPECT_EQ(created->pin.size(), 6u);

  auto s0 = session_manager.GetSessionById(created->session_id);
  ASSERT_TRUE(s0.has_value());
  EXPECT_EQ(s0->state, domain::SessionState::Lobby);
  EXPECT_EQ(s0->players.size(), 1u);
  EXPECT_EQ(s0->current_question_index, 0u);
  EXPECT_FALSE(s0->has_question_deadline);

  EXPECT_TRUE(commands.JoinAsPlayer(created->pin, "alice"));
  EXPECT_TRUE(commands.JoinAsPlayer(created->pin, "bob"));

  auto s1 = session_manager.GetSessionById(created->session_id);
  ASSERT_TRUE(s1.has_value());
  EXPECT_EQ(s1->state, domain::SessionState::Lobby);
  EXPECT_EQ(s1->players.size(), 3u);

  EXPECT_FALSE(commands.JoinAsPlayer("000000", "eve")) << "wrong PIN must be rejected";

  EXPECT_TRUE(commands.StartGame(created->session_id));

  auto s2 = session_manager.GetSessionById(created->session_id);
  ASSERT_TRUE(s2.has_value());
  EXPECT_EQ(s2->state, domain::SessionState::Running);
  EXPECT_EQ(s2->current_question_index, 0u);
  EXPECT_TRUE(s2->has_question_deadline);

  domain::PlayerAnswer a0;
  a0.selected_indices = {1};
  a0.time_since_question_start_ms = std::chrono::milliseconds(200);
  EXPECT_TRUE(commands.SubmitAnswer(created->session_id, "alice", a0));

  domain::PlayerAnswer b0;
  b0.selected_indices = {1};
  b0.time_since_question_start_ms = std::chrono::milliseconds(2000);
  EXPECT_TRUE(commands.SubmitAnswer(created->session_id, "bob", b0));

  auto s3 = session_manager.GetSessionById(created->session_id);
  ASSERT_TRUE(s3.has_value());
  int alice_q0 = ScoreOf(*s3, "alice");
  int bob_q0 = ScoreOf(*s3, "bob");
  EXPECT_GT(alice_q0, 0);
  EXPECT_GT(bob_q0, 0);
  EXPECT_GT(alice_q0, bob_q0) << "faster correct answer gets more points";

  domain::PlayerAnswer a0_dup;
  a0_dup.selected_indices = {0};
  a0_dup.time_since_question_start_ms = std::chrono::milliseconds(3000);
  EXPECT_FALSE(commands.SubmitAnswer(created->session_id, "alice", a0_dup))
      << "second answer on same question must be rejected";

  EXPECT_TRUE(commands.NextQuestion(created->session_id));

  auto s4 = session_manager.GetSessionById(created->session_id);
  ASSERT_TRUE(s4.has_value());
  EXPECT_EQ(s4->state, domain::SessionState::Running);
  EXPECT_EQ(s4->current_question_index, 1u);
  for (const auto& p : s4->players)
    EXPECT_FALSE(p.answered_current_question) << "flags reset for new question";

  domain::PlayerAnswer a1;
  a1.selected_indices = {1, 2};
  a1.time_since_question_start_ms = std::chrono::milliseconds(100);
  EXPECT_TRUE(commands.SubmitAnswer(created->session_id, "alice", a1));

  domain::PlayerAnswer b1;
  b1.selected_indices = {0, 2};
  b1.time_since_question_start_ms = std::chrono::milliseconds(500);
  EXPECT_TRUE(commands.SubmitAnswer(created->session_id, "bob", b1));

  auto s5 = session_manager.GetSessionById(created->session_id);
  ASSERT_TRUE(s5.has_value());
  EXPECT_EQ(ScoreOf(*s5, "alice"), alice_q0) << "alice wrong answer, no new points";
  EXPECT_GT(ScoreOf(*s5, "bob"), bob_q0) << "bob correct, gains points";

  EXPECT_TRUE(commands.NextQuestion(created->session_id));

  auto s6 = session_manager.GetSessionById(created->session_id);
  ASSERT_TRUE(s6.has_value());
  EXPECT_EQ(s6->state, domain::SessionState::Finished);
  EXPECT_EQ(s6->current_question_index, 2u);
  EXPECT_GT(ScoreOf(*s6, "bob"), ScoreOf(*s6, "alice")) << "bob wins";

  EXPECT_FALSE(commands.JoinAsPlayer(created->pin, "late"))
      << "join in Running must be rejected";

  domain::PlayerAnswer too_late;
  too_late.selected_indices = {0};
  too_late.time_since_question_start_ms = std::chrono::milliseconds(0);
  EXPECT_FALSE(commands.SubmitAnswer(created->session_id, "bob", too_late))
      << "submit after game finished must be rejected";
}

// Тот же сценарий с выводом состояний в консоль (как в изначальном демо в main).
// Запуск: ./build/tests/QtBoostCMake_tests --gtest_filter="ServerBusinessLogicDemo.VerboseScenario"
TEST(ServerBusinessLogicDemo, VerboseScenario) {
  services::InMemoryQuizStorage quiz_storage;
  services::QuizRegistry quiz_registry{quiz_storage};
  NoOpBroadcastSink broadcast_sink;
  SteadyTimeProvider time_provider;
  services::SessionManager session_manager{quiz_registry, broadcast_sink};
  services::SessionTimerService timer_service{time_provider,
                                              std::chrono::milliseconds{100}};
  app::ServerCommandHandler commands{quiz_registry, session_manager};

  domain::Quiz quiz;
  quiz.title = "Math & Logic";
  quiz.description = "Two questions";
  quiz.questions.push_back(
      {"What is 2+2?", domain::AnswerType::SingleChoice, {"3", "4", "5"}, {1},
       std::chrono::milliseconds(5000)});
  quiz.questions.push_back(
      {"Select even numbers", domain::AnswerType::MultipleChoice, {"2", "3", "4", "5"}, {0, 2},
       std::chrono::milliseconds(8000)});

  auto quiz_code = commands.CreateQuiz(std::move(quiz));
  ASSERT_TRUE(quiz_code.has_value());
  std::cout << "\n[1] Quiz created: code=" << *quiz_code << "\n";

  auto created = commands.CreateSession(*quiz_code, "host1");
  ASSERT_TRUE(created.has_value());
  std::cout << "[2] Session: id=" << created->session_id << " pin=" << created->pin << "\n";

  auto s0 = session_manager.GetSessionById(created->session_id);
  if (s0) PrintSession(*s0, "После создания (только host)");

  ASSERT_TRUE(commands.JoinAsPlayer(created->pin, "alice"));
  ASSERT_TRUE(commands.JoinAsPlayer(created->pin, "bob"));
  std::cout << "[3] alice, bob joined\n";
  auto s1 = session_manager.GetSessionById(created->session_id);
  if (s1) PrintSession(*s1, "Лобби: host + alice + bob");

  EXPECT_FALSE(commands.JoinAsPlayer("000000", "eve"));
  std::cout << "[4] Wrong PIN rejected\n";

  ASSERT_TRUE(commands.StartGame(created->session_id));
  std::cout << "[5] Game started\n";
  auto s2 = session_manager.GetSessionById(created->session_id);
  if (s2) PrintSession(*s2, "Running, вопрос 0");

  domain::PlayerAnswer a0{{1}, std::chrono::milliseconds(200)};
  domain::PlayerAnswer b0{{1}, std::chrono::milliseconds(2000)};
  ASSERT_TRUE(commands.SubmitAnswer(created->session_id, "alice", a0));
  ASSERT_TRUE(commands.SubmitAnswer(created->session_id, "bob", b0));
  std::cout << "[6] Answers on question 0\n";
  auto s3 = session_manager.GetSessionById(created->session_id);
  if (s3) PrintSession(*s3, "После ответов на вопрос 0");

  domain::PlayerAnswer a0_dup{{0}, std::chrono::milliseconds(3000)};
  EXPECT_FALSE(commands.SubmitAnswer(created->session_id, "alice", a0_dup));
  std::cout << "[7] Second answer rejected\n";

  ASSERT_TRUE(commands.NextQuestion(created->session_id));
  std::cout << "[8] NextQuestion → вопрос 1\n";
  auto s4 = session_manager.GetSessionById(created->session_id);
  if (s4) PrintSession(*s4, "Running, вопрос 1");

  domain::PlayerAnswer a1{{1, 2}, std::chrono::milliseconds(100)};
  domain::PlayerAnswer b1{{0, 2}, std::chrono::milliseconds(500)};
  ASSERT_TRUE(commands.SubmitAnswer(created->session_id, "alice", a1));
  ASSERT_TRUE(commands.SubmitAnswer(created->session_id, "bob", b1));
  std::cout << "[9] Answers on question 1\n";
  auto s5 = session_manager.GetSessionById(created->session_id);
  if (s5) PrintSession(*s5, "После ответов на вопрос 1");

  ASSERT_TRUE(commands.NextQuestion(created->session_id));
  std::cout << "[10] NextQuestion → Finished\n";
  auto s6 = session_manager.GetSessionById(created->session_id);
  if (s6) PrintSession(*s6, "Итог: Finished");

  EXPECT_FALSE(commands.JoinAsPlayer(created->pin, "late"));
  domain::PlayerAnswer too_late{{0}, std::chrono::milliseconds(0)};
  EXPECT_FALSE(commands.SubmitAnswer(created->session_id, "bob", too_late));
  std::cout << "[11] Join/Submit after finish rejected\n";
}

}  // namespace quizlyx::server
