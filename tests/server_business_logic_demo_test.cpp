#include <chrono>
#include <iostream>
#include <string>
#include <unordered_set>

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

// Test constants
constexpr int KTimerIntervalMs = 100;
constexpr int KQ1TimeLimitMs = 5000;
constexpr int KQ2TimeLimitMs = 8000;
constexpr int KAliceQ1TimeMs = 200;
constexpr int KBobQ1TimeMs = 2000;
constexpr int KDupAnswerTimeMs = 3000;
constexpr int KAliceQ2TimeMs = 100;
constexpr int KBobQ2TimeMs = 500;

class SteadyTimeProvider : public interfaces::ITimeProvider {
public:
  [[nodiscard]] std::chrono::steady_clock::time_point Now() const override {
    return std::chrono::steady_clock::now();
  }
};

class NoOpBroadcastSink : public interfaces::IBroadcastSink {
public:
  void Broadcast(const std::string& /*session_id*/, const events::GameEvent& /*event*/) override {
  }
};

static int ScoreOf(const domain::Session& s, const std::string& player_id) {
  for (const auto& p : s.players)
    if (p.id == player_id)
      return p.score;
  return -1;
}

static const char* StateStr(domain::SessionState s) {
  switch (s) {
    case domain::SessionState::Lobby:
      return "Lobby";
    case domain::SessionState::Running:
      return "Running";
    case domain::SessionState::Finished:
      return "Finished";
  }
  return "?";
}

static const char* RoleStr(domain::Role r) {
  return r == domain::Role::Host ? "Host" : "Player";
}

static void PrintSession(const domain::Session& session, const std::string& title) {
  std::cout << "\n--- " << title << " ---\n";
  std::cout << "  state=" << StateStr(session.state) << "  question=" << session.current_question_index
            << "  has_deadline=" << (session.has_question_deadline ? "yes" : "no") << "\n";
  std::cout << "  players:\n";
  for (const auto& p : session.players) {
    std::cout << "    " << p.id << " (" << p.name << ") [" << RoleStr(p.role) << "] score=" << p.score
              << "  answered_current=" << (p.answered_current_question ? "yes" : "no") << "\n";
  }
  std::cout << "---\n";
}

TEST(ServerBusinessLogicDemo, FullScenario) {
  services::InMemoryQuizStorage quiz_storage;
  services::QuizRegistry quiz_registry{quiz_storage};
  NoOpBroadcastSink broadcast_sink;
  SteadyTimeProvider time_provider;
  services::SessionManager session_manager{quiz_registry, broadcast_sink, time_provider};
  services::SessionTimerService timer_service{time_provider, std::chrono::milliseconds{KTimerIntervalMs}};
  app::ServerCommandHandler commands{quiz_registry, session_manager};

  domain::Quiz quiz;
  quiz.title = "Math & Logic";
  quiz.description = "Two questions";
  quiz.questions.push_back({"What is 2+2?",
                            domain::AnswerType::SingleChoice,
                            {"3", "4", "5"},
                            {1},
                            std::chrono::milliseconds(KQ1TimeLimitMs)});
  quiz.questions.push_back({"Select even numbers",
                            domain::AnswerType::MultipleChoice,
                            {"2", "3", "4", "5"},
                            {0, 2},
                            std::chrono::milliseconds(KQ2TimeLimitMs)});

  auto quiz_code_opt = commands.CreateQuiz(std::move(quiz));
  ASSERT_TRUE(quiz_code_opt.has_value()) << "CreateQuiz";
  const auto& quiz_code = quiz_code_opt.value(); // NOLINT(bugprone-unchecked-optional-access)
  EXPECT_FALSE(quiz_code.empty());

  auto created_opt = commands.CreateSession(quiz_code, "host1", "Host", false, 0);
  ASSERT_TRUE(created_opt.has_value()) << "CreateSession";
  const auto& created = created_opt.value(); // NOLINT(bugprone-unchecked-optional-access)
  EXPECT_FALSE(created.session_id.empty());
  EXPECT_EQ(created.pin.size(), 6u);
  EXPECT_EQ(created.player_id, "host1");
  EXPECT_EQ(created.display_name, "Host");

  auto s0_opt = session_manager.GetSessionById(created.session_id);
  ASSERT_TRUE(s0_opt.has_value());
  const auto& s0 = s0_opt.value(); // NOLINT(bugprone-unchecked-optional-access)
  EXPECT_EQ(s0.state, domain::SessionState::Lobby);
  EXPECT_EQ(s0.players.size(), 1u);
  EXPECT_EQ(s0.current_question_index, 0u);
  EXPECT_FALSE(s0.has_question_deadline);
  EXPECT_EQ(s0.players.front().name, "Host");

  std::string alice_id = "alice";
  std::string bob_id = "bob";
  ASSERT_TRUE(commands.JoinAsPlayer(created.pin, alice_id, "Alice").has_value());
  ASSERT_TRUE(commands.JoinAsPlayer(created.pin, bob_id, "Bob").has_value());

  auto s1_opt = session_manager.GetSessionById(created.session_id);
  ASSERT_TRUE(s1_opt.has_value());
  const auto& s1 = s1_opt.value(); // NOLINT(bugprone-unchecked-optional-access)
  EXPECT_EQ(s1.state, domain::SessionState::Lobby);
  EXPECT_EQ(s1.players.size(), 3u);

  EXPECT_FALSE(commands.JoinAsPlayer("000000", "eve", "Eve").has_value()) << "wrong PIN must be rejected";

  EXPECT_TRUE(commands.StartGame(created.session_id));

  auto s2_opt = session_manager.GetSessionById(created.session_id);
  ASSERT_TRUE(s2_opt.has_value());
  const auto& s2 = s2_opt.value(); // NOLINT(bugprone-unchecked-optional-access)
  EXPECT_EQ(s2.state, domain::SessionState::Running);
  EXPECT_EQ(s2.current_question_index, 0u);
  EXPECT_TRUE(s2.has_question_deadline);

  domain::PlayerAnswer a0;
  a0.selected_indices = {1};
  a0.time_since_question_start_ms = std::chrono::milliseconds(KAliceQ1TimeMs);
  EXPECT_TRUE(commands.SubmitAnswer(created.session_id, alice_id, a0));

  domain::PlayerAnswer b0;
  b0.selected_indices = {1};
  b0.time_since_question_start_ms = std::chrono::milliseconds(KBobQ1TimeMs);
  EXPECT_TRUE(commands.SubmitAnswer(created.session_id, bob_id, b0));

  auto s3_opt = session_manager.GetSessionById(created.session_id);
  ASSERT_TRUE(s3_opt.has_value());
  const auto& s3 = s3_opt.value(); // NOLINT(bugprone-unchecked-optional-access)
  int alice_q0 = ScoreOf(s3, alice_id);
  int bob_q0 = ScoreOf(s3, bob_id);
  EXPECT_GT(alice_q0, 0);
  EXPECT_GT(bob_q0, 0);
  EXPECT_GT(alice_q0, bob_q0) << "faster correct answer gets more points";

  domain::PlayerAnswer a0_dup;
  a0_dup.selected_indices = {0};
  a0_dup.time_since_question_start_ms = std::chrono::milliseconds(KDupAnswerTimeMs);
  EXPECT_FALSE(commands.SubmitAnswer(created.session_id, alice_id, a0_dup))
      << "second answer on same question must be rejected";

  auto round1_opt = session_manager.CompleteQuestion(created.session_id);
  ASSERT_TRUE(round1_opt.has_value());
  const auto* round1_reveal = std::get_if<events::AnswerReveal>(&*round1_opt);
  ASSERT_NE(round1_reveal, nullptr);
  EXPECT_EQ(round1_reveal->correct_indices.size(), 1u);
  EXPECT_EQ(round1_reveal->option_pick_counts.size(), 3);

  auto after_round1_opt = session_manager.FinishReveal(created.session_id);
  ASSERT_TRUE(after_round1_opt.has_value());
  const auto* round1_leaderboard = std::get_if<events::Leaderboard>(&*after_round1_opt);
  ASSERT_NE(round1_leaderboard, nullptr);
  EXPECT_EQ(round1_leaderboard->entries.size(), 3u);
  EXPECT_FALSE(round1_leaderboard->next_round_delay_ms.has_value());

  EXPECT_TRUE(commands.NextQuestion(created.session_id));

  auto s4_opt = session_manager.GetSessionById(created.session_id);
  ASSERT_TRUE(s4_opt.has_value());
  const auto& s4 = s4_opt.value(); // NOLINT(bugprone-unchecked-optional-access)
  EXPECT_EQ(s4.state, domain::SessionState::Running);
  EXPECT_EQ(s4.current_question_index, 1u);
  for (const auto& p : s4.players)
    EXPECT_FALSE(p.answered_current_question) << "flags reset for new question";

  domain::PlayerAnswer a1;
  a1.selected_indices = {1, 2};
  a1.time_since_question_start_ms = std::chrono::milliseconds(KAliceQ2TimeMs);
  EXPECT_TRUE(commands.SubmitAnswer(created.session_id, alice_id, a1));

  domain::PlayerAnswer b1;
  b1.selected_indices = {0, 2};
  b1.time_since_question_start_ms = std::chrono::milliseconds(KBobQ2TimeMs);
  EXPECT_TRUE(commands.SubmitAnswer(created.session_id, bob_id, b1));

  auto s5_opt = session_manager.GetSessionById(created.session_id);
  ASSERT_TRUE(s5_opt.has_value());
  const auto& s5 = s5_opt.value(); // NOLINT(bugprone-unchecked-optional-access)
  EXPECT_EQ(ScoreOf(s5, alice_id), alice_q0) << "alice wrong answer, no new points";
  EXPECT_GT(ScoreOf(s5, bob_id), bob_q0) << "bob correct, gains points";

  auto round2_opt = session_manager.CompleteQuestion(created.session_id);
  ASSERT_TRUE(round2_opt.has_value());
  const auto* round2_reveal = std::get_if<events::AnswerReveal>(&*round2_opt);
  ASSERT_NE(round2_reveal, nullptr);
  EXPECT_EQ(round2_reveal->correct_indices.size(), 2u);

  auto finished_opt = session_manager.FinishReveal(created.session_id);
  ASSERT_TRUE(finished_opt.has_value());
  const auto* finished = std::get_if<events::GameFinished>(&*finished_opt);
  ASSERT_NE(finished, nullptr);
  EXPECT_EQ(finished->final_leaderboard.size(), 3u);

  auto s6_opt = session_manager.GetSessionById(created.session_id);
  ASSERT_TRUE(s6_opt.has_value());
  const auto& s6 = s6_opt.value(); // NOLINT(bugprone-unchecked-optional-access)
  EXPECT_EQ(s6.state, domain::SessionState::Finished);
  EXPECT_EQ(s6.current_question_index, 1u);
  EXPECT_GT(ScoreOf(s6, bob_id), ScoreOf(s6, alice_id)) << "bob wins";

  EXPECT_FALSE(commands.JoinAsPlayer(created.pin, "late", "Late").has_value()) << "join in Finished must be rejected";

  domain::PlayerAnswer too_late;
  too_late.selected_indices = {0};
  too_late.time_since_question_start_ms = std::chrono::milliseconds(0);
  EXPECT_FALSE(commands.SubmitAnswer(created.session_id, bob_id, too_late))
      << "submit after game finished must be rejected";
}

TEST(ServerBusinessLogicDemo, VerboseScenario) {
  services::InMemoryQuizStorage quiz_storage;
  services::QuizRegistry quiz_registry{quiz_storage};
  NoOpBroadcastSink broadcast_sink;
  SteadyTimeProvider time_provider;
  services::SessionManager session_manager{quiz_registry, broadcast_sink, time_provider};
  services::SessionTimerService timer_service{time_provider, std::chrono::milliseconds{KTimerIntervalMs}};
  app::ServerCommandHandler commands{quiz_registry, session_manager};

  domain::Quiz quiz;
  quiz.title = "Math & Logic";
  quiz.description = "Two questions";
  quiz.questions.push_back({"What is 2+2?",
                            domain::AnswerType::SingleChoice,
                            {"3", "4", "5"},
                            {1},
                            std::chrono::milliseconds(KQ1TimeLimitMs)});
  quiz.questions.push_back({"Select even numbers",
                            domain::AnswerType::MultipleChoice,
                            {"2", "3", "4", "5"},
                            {0, 2},
                            std::chrono::milliseconds(KQ2TimeLimitMs)});

  auto quiz_code_opt = commands.CreateQuiz(std::move(quiz));
  ASSERT_TRUE(quiz_code_opt.has_value());
  const auto& quiz_code = quiz_code_opt.value(); // NOLINT(bugprone-unchecked-optional-access)
  std::cout << "\n[1] Quiz created: code=" << quiz_code << "\n";

  auto created_opt = commands.CreateSession(quiz_code, "host1", "Host", false, 0);
  ASSERT_TRUE(created_opt.has_value());
  const auto& created = created_opt.value(); // NOLINT(bugprone-unchecked-optional-access)
  std::cout << "[2] Session: id=" << created.session_id << " pin=" << created.pin << "\n";

  auto s0_opt = session_manager.GetSessionById(created.session_id);
  if (s0_opt) {
    const auto& s0 = *s0_opt;
    PrintSession(s0, "After creation (host only)");
  }

  std::string alice_id = "alice_v";
  std::string bob_id = "bob_v";
  ASSERT_TRUE(commands.JoinAsPlayer(created.pin, alice_id, "Alice").has_value());
  ASSERT_TRUE(commands.JoinAsPlayer(created.pin, bob_id, "Bob").has_value());
  std::cout << "[3] Alice (" << alice_id << "), Bob (" << bob_id << ") joined\n";
  auto s1_opt = session_manager.GetSessionById(created.session_id);
  if (s1_opt) {
    const auto& s1 = *s1_opt;
    PrintSession(s1, "Lobby: host + Alice + Bob");
  }

  EXPECT_FALSE(commands.JoinAsPlayer("000000", "eve_v", "Eve").has_value());
  std::cout << "[4] Wrong PIN rejected\n";

  ASSERT_TRUE(commands.StartGame(created.session_id));
  std::cout << "[5] Game started\n";
  auto s2_opt = session_manager.GetSessionById(created.session_id);
  if (s2_opt) {
    const auto& s2 = *s2_opt;
    PrintSession(s2, "Running, question 0");
  }

  domain::PlayerAnswer a0{.selected_indices = {1},
                          .time_since_question_start_ms = std::chrono::milliseconds(KAliceQ1TimeMs)};
  domain::PlayerAnswer b0{.selected_indices = {1},
                          .time_since_question_start_ms = std::chrono::milliseconds(KBobQ1TimeMs)};
  ASSERT_TRUE(commands.SubmitAnswer(created.session_id, alice_id, a0));
  ASSERT_TRUE(commands.SubmitAnswer(created.session_id, bob_id, b0));
  std::cout << "[6] Answers on question 0\n";
  auto s3_opt = session_manager.GetSessionById(created.session_id);
  if (s3_opt) {
    const auto& s3 = *s3_opt;
    PrintSession(s3, "After answers on question 0");
  }

  domain::PlayerAnswer a0_dup{.selected_indices = {0},
                              .time_since_question_start_ms = std::chrono::milliseconds(KDupAnswerTimeMs)};
  EXPECT_FALSE(commands.SubmitAnswer(created.session_id, alice_id, a0_dup));
  std::cout << "[7] Second answer rejected\n";

  auto round1_opt = session_manager.CompleteQuestion(created.session_id);
  ASSERT_TRUE(round1_opt.has_value());
  ASSERT_NE(std::get_if<events::AnswerReveal>(&*round1_opt), nullptr);
  std::cout << "[8] CompleteQuestion -> AnswerReveal\n";

  auto after_round1_opt = session_manager.FinishReveal(created.session_id);
  ASSERT_TRUE(after_round1_opt.has_value());
  ASSERT_NE(std::get_if<events::Leaderboard>(&*after_round1_opt), nullptr);
  std::cout << "[9] FinishReveal -> Leaderboard\n";

  ASSERT_TRUE(commands.NextQuestion(created.session_id));
  std::cout << "[10] NextQuestion -> question 1\n";
  auto s4_opt = session_manager.GetSessionById(created.session_id);
  if (s4_opt) {
    const auto& s4 = *s4_opt;
    PrintSession(s4, "Running, question 1");
  }

  domain::PlayerAnswer a1{.selected_indices = {1, 2},
                          .time_since_question_start_ms = std::chrono::milliseconds(KAliceQ2TimeMs)};
  domain::PlayerAnswer b1{.selected_indices = {0, 2},
                          .time_since_question_start_ms = std::chrono::milliseconds(KBobQ2TimeMs)};
  ASSERT_TRUE(commands.SubmitAnswer(created.session_id, alice_id, a1));
  ASSERT_TRUE(commands.SubmitAnswer(created.session_id, bob_id, b1));
  std::cout << "[11] Answers on question 1\n";
  auto s5_opt = session_manager.GetSessionById(created.session_id);
  if (s5_opt) {
    const auto& s5 = *s5_opt;
    PrintSession(s5, "After answers on question 1");
  }

  auto round2_opt = session_manager.CompleteQuestion(created.session_id);
  ASSERT_TRUE(round2_opt.has_value());
  ASSERT_NE(std::get_if<events::AnswerReveal>(&*round2_opt), nullptr);
  std::cout << "[12] CompleteQuestion -> AnswerReveal\n";

  auto finished_opt = session_manager.FinishReveal(created.session_id);
  ASSERT_TRUE(finished_opt.has_value());
  ASSERT_NE(std::get_if<events::GameFinished>(&*finished_opt), nullptr);
  std::cout << "[13] FinishReveal -> GameFinished\n";
  auto s6_opt = session_manager.GetSessionById(created.session_id);
  if (s6_opt) {
    const auto& s6 = *s6_opt;
    PrintSession(s6, "Final: Finished");
  }

  EXPECT_FALSE(commands.JoinAsPlayer(created.pin, "late_v", "Late").has_value());
  domain::PlayerAnswer too_late{.selected_indices = {0}, .time_since_question_start_ms = std::chrono::milliseconds(0)};
  EXPECT_FALSE(commands.SubmitAnswer(created.session_id, bob_id, too_late));
  std::cout << "[14] Join/Submit after finish rejected\n";
}

TEST(ServerBusinessLogicDemo, SpectatorHostExcludedFromRevealCountsAndStandings) {
  services::InMemoryQuizStorage quiz_storage;
  services::QuizRegistry quiz_registry{quiz_storage};
  NoOpBroadcastSink broadcast_sink;
  SteadyTimeProvider time_provider;
  services::SessionManager session_manager{quiz_registry, broadcast_sink, time_provider};
  app::ServerCommandHandler commands{quiz_registry, session_manager};

  domain::Quiz quiz;
  quiz.title = "Spectator";
  quiz.questions.push_back(
      {"Pick four", domain::AnswerType::SingleChoice, {"3", "4", "5"}, {1}, std::chrono::milliseconds(1000)});

  auto quiz_code_opt = commands.CreateQuiz(std::move(quiz));
  ASSERT_TRUE(quiz_code_opt.has_value());

  auto created_opt = commands.CreateSession(*quiz_code_opt, "host1", "Host", true, 0);
  ASSERT_TRUE(created_opt.has_value());
  EXPECT_FALSE(created_opt->is_competing);

  auto before_join_opt = session_manager.GetSessionById(created_opt->session_id);
  ASSERT_TRUE(before_join_opt.has_value());
  ASSERT_EQ(before_join_opt->players.size(), 1u);
  EXPECT_FALSE(before_join_opt->players.front().is_competing);

  EXPECT_FALSE(commands.StartGame(created_opt->session_id)) << "spectator host alone cannot start";

  ASSERT_TRUE(commands.JoinAsPlayer(created_opt->pin, "alice", "Alice").has_value());
  ASSERT_TRUE(commands.JoinAsPlayer(created_opt->pin, "bob", "Bob").has_value());
  ASSERT_TRUE(commands.StartGame(created_opt->session_id));

  domain::PlayerAnswer alice_answer;
  alice_answer.selected_indices = {1};
  alice_answer.time_since_question_start_ms = std::chrono::milliseconds(120);
  EXPECT_TRUE(commands.SubmitAnswer(created_opt->session_id, "alice", alice_answer));

  domain::PlayerAnswer bob_answer;
  bob_answer.selected_indices = {2};
  bob_answer.time_since_question_start_ms = std::chrono::milliseconds(240);
  EXPECT_TRUE(commands.SubmitAnswer(created_opt->session_id, "bob", bob_answer));

  domain::PlayerAnswer host_answer;
  host_answer.selected_indices = {1};
  host_answer.time_since_question_start_ms = std::chrono::milliseconds(50);
  EXPECT_FALSE(commands.SubmitAnswer(created_opt->session_id, "host1", host_answer))
      << "spectator host must not submit answers";

  auto reveal_opt = session_manager.CompleteQuestion(created_opt->session_id);
  ASSERT_TRUE(reveal_opt.has_value());
  const auto* reveal = std::get_if<events::AnswerReveal>(&*reveal_opt);
  ASSERT_NE(reveal, nullptr);
  EXPECT_EQ(reveal->correct_indices, std::vector<size_t>({1}));
  EXPECT_EQ(reveal->option_pick_counts, std::vector<int>({0, 1, 1}));
  EXPECT_EQ(reveal->selections_by_player.size(), 2u);
  EXPECT_FALSE(reveal->selections_by_player.contains("host1"));

  auto finished_opt = session_manager.FinishReveal(created_opt->session_id);
  ASSERT_TRUE(finished_opt.has_value());
  const auto* finished = std::get_if<events::GameFinished>(&*finished_opt);
  ASSERT_NE(finished, nullptr);
  ASSERT_EQ(finished->final_leaderboard.size(), 2u);

  std::unordered_set<std::string> finalists;
  for (const auto& entry : finished->final_leaderboard) {
    finalists.insert(entry.player_id);
  }
  EXPECT_TRUE(finalists.contains("alice"));
  EXPECT_TRUE(finalists.contains("bob"));
  EXPECT_FALSE(finalists.contains("host1"));
}

} // namespace quizlyx::server
