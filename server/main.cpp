#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <random>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

#include "app/command_handler.hpp"
#include "domain/answer.hpp"
#include "domain/quiz.hpp"
#include "interfaces/ibroadcast_sink.hpp"
#include "interfaces/itime_provider.hpp"
#include "services/in_memory_quiz_storage.hpp"
#include "services/quiz_registry.hpp"
#include "services/session_manager.hpp"
#include "services/session_timer_service.hpp"

namespace quizlyx::server {

// Constants for demo timing and display
constexpr int kSessionIdDisplayLength = 6;
constexpr int kTimerUpdateIntervalMs = 200;
constexpr int kWorkerStartDelayMs = 300;
constexpr int kWorkerCount = 4;
constexpr int kGameCount = 3;
constexpr int kClientStartDelayMs = 200;
constexpr int kQuiz1QuestionTimeMs = 3000;
constexpr int kQuiz2QuestionTimeMs = 4000;
constexpr int kQuiz3QuestionTimeMs = 2000;
constexpr int kSessionCreateDelayMs = 100;
constexpr int kPlayerJoinDelayMs = 50;
constexpr int kStartGameDelayMs = 200;
constexpr int kAfterStartDelayMs = 500;
constexpr int kQuestionPlayDelayMs = 500;
constexpr int kAnswerMinDelayMs = 100;
constexpr int kAnswerMaxDelayMs = 400;
constexpr int kAnswerSubmitDelayMs = 100;
constexpr int kAfterAnswersDelayMs = 500;
constexpr int kNextQuestionDelayMs = 200;

class Logger {
public:
  static Logger& Instance() {
    static Logger instance;
    return instance;
  }

  void Log(const std::string& prefix, const std::string& message) {
    std::lock_guard lock(mutex_);
    std::ostringstream tid;
    tid << std::this_thread::get_id();
    std::cout << "[Worker-" << tid.str().substr(tid.str().size() - 4) << "][" << prefix << "] " << message << '\n';
  }

private:
  std::mutex mutex_;
};

// Commands for the queue
struct CreateSessionCmd {
  std::string quiz_code;
  std::string host_id;
  int game_num;
};

struct JoinPlayerCmd {
  std::string pin;
  std::string player_id;
  int game_num;
};

struct StartGameCmd {
  std::string session_id;
  int game_num;
};

struct SubmitAnswerCmd {
  std::string session_id;
  std::string player_id;
  domain::PlayerAnswer answer;
  int game_num;
};

struct NextQuestionCmd {
  std::string session_id;
  int game_num;
};

using Command = std::variant<CreateSessionCmd, JoinPlayerCmd, StartGameCmd, SubmitAnswerCmd, NextQuestionCmd>;

// Thread-safe command queue
class CommandQueue {
public:
  void Push(Command cmd) {
    std::lock_guard lock(mutex_);
    queue_.push(std::move(cmd));
    cv_.notify_one();
  }

  Command Pop() {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [this] { return !queue_.empty() || stop_; });

    if (stop_ && queue_.empty()) {
      return CreateSessionCmd{}; // dummy
    }

    Command cmd = std::move(queue_.front());
    queue_.pop();
    return cmd;
  }

  void Stop() {
    std::lock_guard lock(mutex_);
    stop_ = true;
    cv_.notify_all();
  }

  bool IsStopped() const {
    std::lock_guard lock(mutex_);
    return stop_;
  }

private:
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<Command> queue_;
  bool stop_ = false;
};

class SteadyTimeProvider : public interfaces::ITimeProvider {
public:
  [[nodiscard]] std::chrono::steady_clock::time_point Now() const override {
    return std::chrono::steady_clock::now();
  }
};

class LoggingBroadcastSink : public interfaces::IBroadcastSink {
public:
  void Broadcast(const std::string& session_id, const events::GameEvent& event) override {
    std::ostringstream msg;
    msg << "Session[" << session_id.substr(0, kSessionIdDisplayLength) << "] ";

    std::visit(
        [&msg](auto&& ev) {
          using T = std::decay_t<decltype(ev)>;
          if constexpr (std::is_same_v<T, events::QuestionStarted>) {
            msg << "QuestionStarted #" << ev.question_index;
          } else if constexpr (std::is_same_v<T, events::TimerUpdate>) {
            msg << "TimerUpdate " << ev.remaining_ms.count() << "ms";
          } else if constexpr (std::is_same_v<T, events::QuestionTimeout>) {
            msg << "QuestionTimeout";
          } else if constexpr (std::is_same_v<T, events::Leaderboard>) {
            msg << "Leaderboard";
          } else if constexpr (std::is_same_v<T, events::GameFinished>) {
            msg << "GameFinished";
          }
        },
        event);

    Logger::Instance().Log("EVENT", msg.str());
  }
};

// Storage for session info across commands
struct SessionInfo {
  std::string session_id;
  std::string pin;
  std::string quiz_code;
};

class Demo {
public:
  Demo() :
      session_manager_(quiz_registry_, broadcast_sink_),
      timer_service_(time_provider_, std::chrono::milliseconds{kTimerUpdateIntervalMs}),
      commands_(quiz_registry_, session_manager_), stop_flag_(false) {
  }

  void Run() {
    std::cout << "\n=== Worker Pool Architecture Demo ===\n" << '\n';

    CreateQuizzes();

    // Start Timer Thread
    std::thread timer_thread([this]() { RunTimerThread(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(kWorkerStartDelayMs));

    // Start Worker Pool (4 workers)
    std::vector<std::thread> workers;
    workers.reserve(kWorkerCount);
    for (int i = 0; i < kWorkerCount; ++i) {
      workers.emplace_back([this, i]() { RunWorker(i); });
    }
    Logger::Instance().Log("SETUP", "Worker pool started (4 workers)");

    std::this_thread::sleep_for(std::chrono::milliseconds(kWorkerStartDelayMs));

    // Simulate 3 games with client threads
    std::vector<std::thread> client_threads;
    for (int i = 0; i < kGameCount; ++i) {
      client_threads.emplace_back([this, i]() { SimulateClient(i); });
      std::this_thread::sleep_for(std::chrono::milliseconds(kClientStartDelayMs));
    }

    // Wait for clients to finish
    for (auto& t : client_threads) {
      t.join();
    }

    std::cout << "\n=== All games completed ===\n" << '\n';

    // Stop workers
    command_queue_.Stop();
    for (auto& w : workers) {
      w.join();
    }

    stop_flag_ = true;
    timer_thread.join();

    Logger::Instance().Log("SHUTDOWN", "All threads stopped");
  }

private:
  void CreateQuizzes() {
    domain::Quiz quiz1;
    quiz1.title = "Speed Math";
    quiz1.questions.push_back({"2+2=?",
                               domain::AnswerType::SingleChoice,
                               {"3", "4", "5"},
                               {1},
                               std::chrono::milliseconds(kQuiz1QuestionTimeMs)});
    quiz1.questions.push_back({"5*2=?",
                               domain::AnswerType::SingleChoice,
                               {"8", "10", "12"},
                               {1},
                               std::chrono::milliseconds(kQuiz1QuestionTimeMs)});
    auto quiz1_code = commands_.CreateQuiz(std::move(quiz1));
    if (quiz1_code) {
      quiz1_code_ = *quiz1_code;
    }

    domain::Quiz quiz2;
    quiz2.title = "Logic";
    quiz2.questions.push_back({"Primes?",
                               domain::AnswerType::MultipleChoice,
                               {"2", "3", "4", "6"},
                               {0, 1},
                               std::chrono::milliseconds(kQuiz2QuestionTimeMs)});
    auto quiz2_code = commands_.CreateQuiz(std::move(quiz2));
    if (quiz2_code) {
      quiz2_code_ = *quiz2_code;
    }

    domain::Quiz quiz3;
    quiz3.title = "Quick";
    quiz3.questions.push_back({"1+1=?",
                               domain::AnswerType::SingleChoice,
                               {"1", "2", "3"},
                               {1},
                               std::chrono::milliseconds(kQuiz3QuestionTimeMs)});
    auto quiz3_code = commands_.CreateQuiz(std::move(quiz3));
    if (quiz3_code) {
      quiz3_code_ = *quiz3_code;
    }

    Logger::Instance().Log("SETUP", "Created 3 quizzes");
  }

  void RunWorker(int worker_id) {
    std::ostringstream tid;
    tid << std::this_thread::get_id();
    Logger::Instance().Log("WORKER" + std::to_string(worker_id),
                           "Started (tid=" + tid.str().substr(tid.str().size() - 4) + ")");

    while (!command_queue_.IsStopped()) {
      Command cmd = command_queue_.Pop();

      if (command_queue_.IsStopped())
        break;

      std::visit(
          [this, worker_id](auto&& command) {
            using T = std::decay_t<decltype(command)>;

            if constexpr (std::is_same_v<T, CreateSessionCmd>) {
              auto result = commands_.CreateSession(command.quiz_code, command.host_id);
              if (result) {
                std::lock_guard lock(sessions_mutex_);
                session_info_[command.game_num] = {result->session_id, result->pin, command.quiz_code};
                Logger::Instance().Log("CMD",
                                       "Game" + std::to_string(command.game_num) + " CreateSession " + result->pin);
              }
            } else if constexpr (std::is_same_v<T, JoinPlayerCmd>) {
              commands_.JoinAsPlayer(command.pin, command.player_id);
              Logger::Instance().Log("CMD",
                                     "Game" + std::to_string(command.game_num) + " JoinPlayer " + command.player_id);
            } else if constexpr (std::is_same_v<T, StartGameCmd>) {
              commands_.StartGame(command.session_id);

              auto session = session_manager_.GetSessionById(command.session_id);
              if (session && session->has_question_deadline) {
                timer_service_.SetDeadline(command.session_id, session->question_deadline);
              }

              Logger::Instance().Log("CMD", "Game" + std::to_string(command.game_num) + " StartGame");
            } else if constexpr (std::is_same_v<T, SubmitAnswerCmd>) {
              commands_.SubmitAnswer(command.session_id, command.player_id, command.answer);
              Logger::Instance().Log("CMD",
                                     "Game" + std::to_string(command.game_num) + " SubmitAnswer " + command.player_id);
            } else if constexpr (std::is_same_v<T, NextQuestionCmd>) {
              timer_service_.ClearDeadline(command.session_id);
              commands_.NextQuestion(command.session_id);

              auto session = session_manager_.GetSessionById(command.session_id);
              if (session && session->has_question_deadline) {
                timer_service_.SetDeadline(command.session_id, session->question_deadline);
              }

              Logger::Instance().Log("CMD", "Game" + std::to_string(command.game_num) + " NextQuestion");
            }
          },
          cmd);
    }

    Logger::Instance().Log("WORKER" + std::to_string(worker_id), "Stopped");
  }

  void SimulateClient(int game_num) {
    std::string quiz_code = (game_num % 3 == 0) ? quiz1_code_ : (game_num % 3 == 1) ? quiz2_code_ : quiz3_code_;

    // Create session
    command_queue_.Push(CreateSessionCmd{
        .quiz_code = quiz_code, .host_id = "host_" + std::to_string(game_num), .game_num = game_num});
    std::this_thread::sleep_for(std::chrono::milliseconds(kSessionCreateDelayMs));

    // Get session info
    SessionInfo info;
    {
      std::lock_guard lock(sessions_mutex_);
      if (session_info_.find(game_num) != session_info_.end()) {
        info = session_info_[game_num];
      }
    }

    // Join players
    std::vector<std::string> players = {"alice", "bob", "charlie"};
    for (const auto& player : players) {
      command_queue_.Push(JoinPlayerCmd{
          .pin = info.pin, .player_id = player + "_g" + std::to_string(game_num), .game_num = game_num});
      std::this_thread::sleep_for(std::chrono::milliseconds(kPlayerJoinDelayMs));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(kStartGameDelayMs));

    // Start game
    command_queue_.Push(StartGameCmd{.session_id = info.session_id, .game_num = game_num});
    std::this_thread::sleep_for(std::chrono::milliseconds(kAfterStartDelayMs));

    // Get number of questions
    auto session = session_manager_.GetSessionById(info.session_id);
    if (!session)
      return;
    auto quiz = quiz_registry_.Get(session->quiz_code);
    if (!quiz)
      return;

    // Play questions
    for (size_t q = 0; q < quiz->questions.size(); ++q) {
      std::this_thread::sleep_for(std::chrono::milliseconds(kQuestionPlayDelayMs));

      // Submit answers
      for (const auto& player : players) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> answer_dist(0, 2);
        std::uniform_int_distribution<> delay_dist(kAnswerMinDelayMs, kAnswerMaxDelayMs);

        domain::PlayerAnswer answer;
        answer.selected_indices = {static_cast<size_t>(answer_dist(gen))};
        answer.time_since_question_start_ms = std::chrono::milliseconds(delay_dist(gen));

        command_queue_.Push(SubmitAnswerCmd{.session_id = info.session_id,
                                            .player_id = player + "_g" + std::to_string(game_num),
                                            .answer = answer,
                                            .game_num = game_num});
        std::this_thread::sleep_for(std::chrono::milliseconds(kAnswerSubmitDelayMs));
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(kAfterAnswersDelayMs));

      // Next question
      command_queue_.Push(NextQuestionCmd{.session_id = info.session_id, .game_num = game_num});
      std::this_thread::sleep_for(std::chrono::milliseconds(kNextQuestionDelayMs));
    }
  }

  void RunTimerThread() {
    Logger::Instance().Log("TIMER", "Started");

    while (!stop_flag_) {
      std::this_thread::sleep_for(std::chrono::milliseconds(kTimerUpdateIntervalMs));

      auto events = timer_service_.Tick();
      for (const auto& timer_event : events) {
        broadcast_sink_.Broadcast(timer_event.session_id, timer_event.event);
      }
    }

    Logger::Instance().Log("TIMER", "Stopped");
  }

  services::InMemoryQuizStorage quiz_storage_;
  services::QuizRegistry quiz_registry_{quiz_storage_};
  LoggingBroadcastSink broadcast_sink_;
  SteadyTimeProvider time_provider_;
  services::SessionManager session_manager_;
  services::SessionTimerService timer_service_;
  app::ServerCommandHandler commands_;

  CommandQueue command_queue_;
  std::atomic<bool> stop_flag_;

  std::mutex sessions_mutex_;
  std::unordered_map<int, SessionInfo> session_info_;

  std::string quiz1_code_;
  std::string quiz2_code_;
  std::string quiz3_code_;
};

} // namespace quizlyx::server

int main(int argc, char** argv) {
  (void) argc;
  (void) argv;

  using namespace quizlyx::server;

  Demo demo;
  demo.Run();

  return 0;
}
