#include <chrono>
#include <iostream>
#include <queue>
#include <string>
#include <thread>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "app/command_handler.hpp"
#include "interfaces/itime_provider.hpp"
#include "network/game_controller.hpp"
#include "network/ws_broadcast_sink.hpp"
#include "network/ws_connection_manager.hpp"
#include "network/ws_server.hpp"
#include "services/in_memory_quiz_storage.hpp"
#include "services/quiz_registry.hpp"
#include "services/session_manager.hpp"
#include "services/session_timer_service.hpp"

namespace quizlyx::server {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using json = nlohmann::json;

constexpr int WsTimeoutSeconds = 10;
constexpr int DefaultTimeLimitMs = 800;
constexpr int TimerIntervalMs = 200;
constexpr int AutoAdvanceDelayMs = 400;
constexpr int MaxExtraPlayers = 49;
constexpr int AnswerTimeFast = 100;
constexpr int AnswerTimeMedium = 150;
constexpr int AnswerTimeNormal = 200;
constexpr int AnswerTimeSlow = 300;
constexpr int AnswerTimeVerySlow = 500;

class TestTimeProvider : public interfaces::ITimeProvider {
public:
  [[nodiscard]] std::chrono::steady_clock::time_point Now() const override {
    return std::chrono::steady_clock::now();
  }
};

// Synchronous WebSocket test client
class TestClient {
public:
  void Connect(uint16_t port) {
    tcp::resolver resolver{ioc_};
    auto results = resolver.resolve("127.0.0.1", std::to_string(port));
    beast::get_lowest_layer(ws_).connect(*results.begin());
    ws_.handshake("127.0.0.1:" + std::to_string(port), "/");
  }

  json SendCommand(const json& msg) {
    ws_.write(net::buffer(msg.dump()));
    return ReadResponse();
  }

  json ReadResponse() {
    while (true) {
      auto j = ReadRaw();
      if (j["type"] == "response")
        return j;
      event_queue_.push(j);
    }
  }

  json ReadEvent() {
    if (!event_queue_.empty()) {
      auto e = event_queue_.front();
      event_queue_.pop();
      return e;
    }
    return ReadRaw();
  }

  void Close() {
    ws_.close(websocket::close_code::normal);
  }
  void Drop() {
    beast::get_lowest_layer(ws_).close();
  }

private:
  json ReadRaw() {
    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(WsTimeoutSeconds));
    beast::flat_buffer buf;
    ws_.read(buf);
    return json::parse(beast::buffers_to_string(buf.data()));
  }

  net::io_context ioc_;
  websocket::stream<beast::tcp_stream> ws_{ioc_};
  std::queue<json> event_queue_;
};

using ClientPtr = std::unique_ptr<TestClient>;

static json WaitForEvent(TestClient& client, const std::string& event_type) {
  while (true) {
    auto event = client.ReadEvent();
    if (event["type"] == "event" && event["event_type"] == event_type)
      return event;
  }
}

static json MakeQuizPayload(const std::string& title, int time_limit_ms = DefaultTimeLimitMs) {
  return {{"title", title},
          {"description", "test quiz"},
          {"questions",
           {{{"text", "2+2=?"},
             {"answer_type", "single_choice"},
             {"options", {"3", "4", "5"}},
             {"correct_indices", {1}},
             {"time_limit_ms", time_limit_ms}},
            {{"text", "3+3=?"},
             {"answer_type", "single_choice"},
             {"options", {"5", "6", "7"}},
             {"correct_indices", {1}},
             {"time_limit_ms", time_limit_ms}}}}};
}

class NetworkTest : public ::testing::Test {
protected:
  void SetUp() override {
    quiz_storage_ = std::make_unique<services::InMemoryQuizStorage>();
    quiz_registry_ = std::make_unique<services::QuizRegistry>(*quiz_storage_);
    time_provider_ = std::make_unique<TestTimeProvider>();
    connection_manager_ = std::make_unique<network::WsConnectionManager>();
    broadcast_sink_ = std::make_unique<network::WsBroadcastSink>(*connection_manager_);
    session_manager_ = std::make_unique<services::SessionManager>(*quiz_registry_, *broadcast_sink_, *time_provider_);
    timer_service_ =
        std::make_unique<services::SessionTimerService>(*time_provider_, std::chrono::milliseconds{TimerIntervalMs});
    command_handler_ = std::make_unique<app::ServerCommandHandler>(*quiz_registry_, *session_manager_);
    game_controller_ = std::make_unique<network::GameController>(*command_handler_, *timer_service_, *session_manager_);
    ioc_ = std::make_unique<net::io_context>();
    auto endpoint = tcp::endpoint(tcp::v4(), 0);
    server_ = std::make_unique<network::WsServer>(*ioc_, endpoint, *game_controller_, *connection_manager_);
    port_ = server_->Port();
    server_->Start();
    tick_timer_ = std::make_shared<net::steady_timer>(*ioc_);
    ScheduleTick();
    server_thread_ = std::thread([this]() { ioc_->run(); });
  }

  void TearDown() override {
    if (tick_timer_)
      tick_timer_->cancel();
    server_->Stop();
    ioc_->stop();
    if (server_thread_.joinable())
      server_thread_.join();
    // Destroy server and io_context while services are still alive.
    // io_context destructor cleans up pending handlers → WsConnection destructors
    // call OnDisconnect → need SessionManager to still exist.
    server_.reset();
    ioc_.reset();
  }

  ClientPtr MakeClient() {
    auto c = std::make_unique<TestClient>();
    c->Connect(port_);
    return c;
  }

  void ScheduleTick() {
    tick_timer_->expires_after(std::chrono::milliseconds{TimerIntervalMs});
    tick_timer_->async_wait([this](const boost::system::error_code& ec) {
      if (ec)
        return;

      auto timer_events = timer_service_->Tick();
      for (const auto& te : timer_events) {
        if (te.timer_type == services::TimerType::QuestionDeadline) {
          if (std::holds_alternative<events::TimerUpdate>(te.event)) {
            broadcast_sink_->Broadcast(te.session_id, te.event);
          } else if (std::holds_alternative<events::QuestionTimeout>(te.event)) {
            broadcast_sink_->Broadcast(te.session_id, te.event);
            auto round_result = game_controller_->CompleteQuestion(te.session_id);
            if (!round_result)
              continue;
            broadcast_sink_->Broadcast(te.session_id, *round_result);
            if (const auto* reveal = std::get_if<events::AnswerReveal>(&*round_result); reveal != nullptr) {
              timer_service_->SetRevealDeadline(te.session_id, time_provider_->Now() + reveal->reveal_duration_ms);
            }
          }
        } else if (te.timer_type == services::TimerType::RevealDelay) {
          auto post_reveal_event = game_controller_->FinishReveal(te.session_id);
          if (!post_reveal_event)
            continue;
          broadcast_sink_->Broadcast(te.session_id, *post_reveal_event);
          if (const auto* leaderboard = std::get_if<events::Leaderboard>(&*post_reveal_event);
              leaderboard != nullptr && leaderboard->next_round_delay_ms.has_value()) {
            timer_service_->SetAutoAdvanceDeadline(te.session_id,
                                                   time_provider_->Now() + *leaderboard->next_round_delay_ms);
          }
        } else if (te.timer_type == services::TimerType::AutoAdvance) {
          game_controller_->NextQuestion(te.session_id);
        }
      }

      ScheduleTick();
    });
  }

  uint16_t port_ = 0;                    // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  std::unique_ptr<net::io_context> ioc_; // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  std::thread server_thread_;            // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  std::unique_ptr<services::InMemoryQuizStorage>
      quiz_storage_; // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  std::unique_ptr<services::QuizRegistry>
      quiz_registry_;                               // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  std::unique_ptr<TestTimeProvider> time_provider_; // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  std::unique_ptr<network::WsConnectionManager>
      connection_manager_; // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  std::unique_ptr<network::WsBroadcastSink>
      broadcast_sink_; // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  std::unique_ptr<services::SessionManager>
      session_manager_; // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  std::unique_ptr<services::SessionTimerService>
      timer_service_; // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  std::unique_ptr<app::ServerCommandHandler>
      command_handler_; // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  std::unique_ptr<network::GameController>
      game_controller_;                           // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  std::unique_ptr<network::WsServer> server_;     // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  std::shared_ptr<net::steady_timer> tick_timer_; // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
};

// ---- 1. Create quiz and session ----
TEST_F(NetworkTest, CreateQuizAndSession) {
  auto h = MakeClient();
  auto qr = h->SendCommand({{"id", "1"}, {"type", "create_quiz"}, {"payload", MakeQuizPayload("Math")}});
  EXPECT_TRUE(qr["success"].get<bool>());
  std::string qc = qr["payload"]["quiz_code"];
  EXPECT_FALSE(qc.empty());
  std::cout << "  Quiz created: " << qc << "\n";

  auto sr = h->SendCommand({{"id", "2"},
                            {"type", "create_session"},
                            {"payload", {{"quiz_code", qc}, {"host_name", "Host One"}, {"host_is_spectator", false}}}});
  EXPECT_TRUE(sr["success"].get<bool>());
  std::string sid = sr["payload"]["session_id"];
  std::string pin = sr["payload"]["pin"];
  std::string pid = sr["payload"]["player_id"];
  std::string display_name = sr["payload"]["display_name"];
  EXPECT_FALSE(sid.empty());
  EXPECT_EQ(pin.size(), 6u);
  EXPECT_FALSE(pid.empty());
  EXPECT_EQ(display_name, "Host One");
  EXPECT_TRUE(sr["payload"]["is_competing"].get<bool>());
  ASSERT_TRUE(sr["payload"].contains("players"));
  ASSERT_EQ(sr["payload"]["players"].size(), 1u);
  EXPECT_EQ(sr["payload"]["players"][0]["display_name"], "Host One");
  EXPECT_EQ(sr["payload"]["players"][0]["role"], "host");
  EXPECT_TRUE(sr["payload"]["players"][0]["is_competing"].get<bool>());
  std::cout << "  Session: id=" << sid << " pin=" << pin << "\n";
  h->Close();
}

TEST_F(NetworkTest, JoinByPinReturnsRosterAndResolvedDisplayNames) {
  auto h = MakeClient();
  auto qr = h->SendCommand({{"id", "1"}, {"type", "create_quiz"}, {"payload", MakeQuizPayload("Names")}});
  ASSERT_TRUE(qr["success"].get<bool>());
  const std::string qc = qr["payload"]["quiz_code"];

  auto sr = h->SendCommand({{"id", "2"},
                            {"type", "create_session"},
                            {"payload", {{"quiz_code", qc}, {"host_name", "Host"}, {"host_is_spectator", false}}}});
  ASSERT_TRUE(sr["success"].get<bool>());
  const std::string pin = sr["payload"]["pin"];

  auto p1 = MakeClient();
  auto jr1 = p1->SendCommand({{"id", "1"}, {"type", "join"}, {"payload", {{"pin", pin}, {"name", "Alex"}}}});
  ASSERT_TRUE(jr1["success"].get<bool>());
  EXPECT_EQ(jr1["payload"]["display_name"], "Alex");
  EXPECT_TRUE(jr1["payload"]["is_competing"].get<bool>());
  auto joined1 = WaitForEvent(*h, "player_joined");
  EXPECT_EQ(joined1["payload"]["display_name"], "Alex");
  EXPECT_TRUE(joined1["payload"]["is_competing"].get<bool>());

  auto p2 = MakeClient();
  auto jr2 = p2->SendCommand({{"id", "1"}, {"type", "join"}, {"payload", {{"pin", pin}, {"name", "Alex"}}}});
  ASSERT_TRUE(jr2["success"].get<bool>());
  EXPECT_EQ(jr2["payload"]["display_name"], "Alex (2)");
  ASSERT_EQ(jr2["payload"]["players"].size(), 3u);
  EXPECT_EQ(jr2["payload"]["players"][1]["display_name"], "Alex");
  EXPECT_EQ(jr2["payload"]["players"][2]["display_name"], "Alex (2)");
  EXPECT_TRUE(jr2["payload"]["players"][2]["is_competing"].get<bool>());

  auto joined2 = WaitForEvent(*h, "player_joined");
  EXPECT_EQ(joined2["payload"]["display_name"], "Alex (2)");
  EXPECT_TRUE(joined2["payload"]["is_competing"].get<bool>());

  h->Close();
  p1->Close();
  p2->Close();
}

TEST_F(NetworkTest, AutoAdvanceLeaderboardIncludesDelayAndAdvances) {
  auto h = MakeClient();
  auto qr = h->SendCommand({{"id", "1"}, {"type", "create_quiz"}, {"payload", MakeQuizPayload("Auto")}});
  ASSERT_TRUE(qr["success"].get<bool>());
  const std::string qc = qr["payload"]["quiz_code"];

  auto sr = h->SendCommand({{"id", "2"},
                            {"type", "create_session"},
                            {"payload",
                             {{"quiz_code", qc},
                              {"host_name", "Host"},
                              {"host_is_spectator", false},
                              {"auto_advance_delay_ms", AutoAdvanceDelayMs}}}});
  ASSERT_TRUE(sr["success"].get<bool>());
  const std::string sid = sr["payload"]["session_id"];
  const std::string pin = sr["payload"]["pin"];

  auto p = MakeClient();
  auto jr = p->SendCommand({{"id", "1"}, {"type", "join"}, {"payload", {{"pin", pin}, {"name", "Alice"}}}});
  ASSERT_TRUE(jr["success"].get<bool>());
  const std::string pid = jr["payload"]["player_id"];
  WaitForEvent(*h, "player_joined");

  auto sgr = h->SendCommand({{"id", "3"}, {"type", "start_game"}, {"payload", {{"session_id", sid}}}});
  ASSERT_TRUE(sgr["success"].get<bool>());
  WaitForEvent(*h, "question_started");
  WaitForEvent(*p, "question_started");

  auto answer = p->SendCommand(
      {{"id", "4"},
       {"type", "submit_answer"},
       {"payload", {{"session_id", sid}, {"player_id", pid}, {"selected_indices", {1}}, {"time_ms", AnswerTimeFast}}}});
  ASSERT_TRUE(answer["success"].get<bool>());

  EXPECT_EQ(WaitForEvent(*h, "question_timeout")["event_type"], "question_timeout");
  auto reveal = WaitForEvent(*h, "answer_reveal");
  EXPECT_EQ(reveal["payload"]["reveal_duration_ms"].get<int>(), 5000);
  auto leaderboard = WaitForEvent(*h, "leaderboard");
  ASSERT_TRUE(leaderboard["payload"].contains("next_round_delay_ms"));
  EXPECT_EQ(leaderboard["payload"]["next_round_delay_ms"].get<int>(), AutoAdvanceDelayMs);

  auto nextQuestion = WaitForEvent(*h, "question_started");
  EXPECT_EQ(nextQuestion["payload"]["question_index"].get<int>(), 1);

  h->Close();
  p->Close();
}

// ---- 2. Full game flow ----
TEST_F(NetworkTest, FullGameFlow) {
  auto h = MakeClient();

  auto qr = h->SendCommand({{"id", "1"}, {"type", "create_quiz"}, {"payload", MakeQuizPayload("Demo")}});
  ASSERT_TRUE(qr["success"].get<bool>());
  std::string qc = qr["payload"]["quiz_code"];

  auto sr = h->SendCommand({{"id", "2"},
                            {"type", "create_session"},
                            {"payload", {{"quiz_code", qc}, {"host_name", "Host"}, {"host_is_spectator", false}}}});
  ASSERT_TRUE(sr["success"].get<bool>());
  std::string sid = sr["payload"]["session_id"];
  std::string pin = sr["payload"]["pin"];
  std::cout << "  Session " << sid << " (pin=" << pin << ")\n";

  // Alice joins
  auto a = MakeClient();
  auto ar = a->SendCommand({{"id", "1"}, {"type", "join"}, {"payload", {{"pin", pin}, {"name", "Alice"}}}});
  ASSERT_TRUE(ar["success"].get<bool>());
  std::string aid = ar["payload"]["player_id"];
  EXPECT_EQ(ar["payload"]["display_name"], "Alice");
  ASSERT_EQ(ar["payload"]["players"].size(), 2u);
  std::cout << "  Alice joined: " << aid << "\n";

  // Host gets PlayerJoined for Alice
  auto he = WaitForEvent(*h, "player_joined");
  EXPECT_EQ(he["event_type"], "player_joined");
  EXPECT_EQ(he["payload"]["player_id"], aid);
  EXPECT_EQ(he["payload"]["display_name"], "Alice");

  // Bob joins
  auto b = MakeClient();
  auto br = b->SendCommand({{"id", "1"}, {"type", "join"}, {"payload", {{"pin", pin}, {"name", "Bob"}}}});
  ASSERT_TRUE(br["success"].get<bool>());
  std::string bid = br["payload"]["player_id"];
  std::cout << "  Bob joined: " << bid << "\n";

  // Host and Alice get PlayerJoined for Bob
  auto he2 = WaitForEvent(*h, "player_joined");
  EXPECT_EQ(he2["event_type"], "player_joined");
  EXPECT_EQ(he2["payload"]["display_name"], "Bob");
  auto ae = WaitForEvent(*a, "player_joined");
  EXPECT_EQ(ae["event_type"], "player_joined");

  // Start game
  auto sgr = h->SendCommand({{"id", "3"}, {"type", "start_game"}, {"payload", {{"session_id", sid}}}});
  EXPECT_TRUE(sgr["success"].get<bool>());
  std::cout << "  Game started\n";

  // All get QuestionStarted
  auto hqs = WaitForEvent(*h, "question_started");
  EXPECT_EQ(hqs["event_type"], "question_started");
  EXPECT_EQ(hqs["payload"]["question_index"], 0);
  EXPECT_EQ(hqs["payload"]["total_questions"], 2);
  EXPECT_EQ(hqs["payload"]["text"], "2+2=?");
  EXPECT_EQ(hqs["payload"]["answer_type"], "single_choice");
  EXPECT_EQ(hqs["payload"]["options"], (json::array({"3", "4", "5"})));
  std::cout << "  Question 0 started (duration=" << hqs["payload"]["duration_ms"] << "ms)\n";
  auto aqs = WaitForEvent(*a, "question_started");
  EXPECT_EQ(aqs["event_type"], "question_started");
  auto bqs = WaitForEvent(*b, "question_started");
  EXPECT_EQ(bqs["event_type"], "question_started");

  // Alice answers correctly (index 1)
  auto aa = a->SendCommand(
      {{"id", "2"},
       {"type", "submit_answer"},
       {"payload",
        {{"session_id", sid}, {"player_id", aid}, {"selected_indices", {1}}, {"time_ms", AnswerTimeNormal}}}});
  EXPECT_TRUE(aa["success"].get<bool>());
  std::cout << "  Alice answered (correct)\n";

  // Bob answers wrong (index 0)
  auto ba = b->SendCommand(
      {{"id", "2"},
       {"type", "submit_answer"},
       {"payload",
        {{"session_id", sid}, {"player_id", bid}, {"selected_indices", {0}}, {"time_ms", AnswerTimeVerySlow}}}});
  EXPECT_TRUE(ba["success"].get<bool>());
  std::cout << "  Bob answered (wrong)\n";

  auto htimeout = WaitForEvent(*h, "question_timeout");
  EXPECT_EQ(htimeout["event_type"], "question_timeout");
  auto hreveal = WaitForEvent(*h, "answer_reveal");
  EXPECT_EQ(hreveal["event_type"], "answer_reveal");
  auto hlb = WaitForEvent(*h, "leaderboard");
  EXPECT_EQ(hlb["event_type"], "leaderboard");
  EXPECT_FALSE(hlb["payload"].contains("next_round_delay_ms"));
  std::cout << "  Leaderboard: ";
  std::unordered_map<std::string, int> scores;
  for (const auto& e : hlb["payload"]["entries"]) {
    scores[e["player_id"].get<std::string>()] = e["score"].get<int>();
    std::cout << e["player_id"].get<std::string>() << "=" << e["score"].get<int>() << " ";
  }
  std::cout << "\n";
  EXPECT_GT(scores[aid], 0);
  EXPECT_EQ(scores[bid], 0);

  // Next question
  auto nr = h->SendCommand({{"id", "4"}, {"type", "next_question"}, {"payload", {{"session_id", sid}}}});
  EXPECT_TRUE(nr["success"].get<bool>());
  auto hqs2 = WaitForEvent(*h, "question_started");
  EXPECT_EQ(hqs2["event_type"], "question_started");
  EXPECT_EQ(hqs2["payload"]["question_index"], 1);
  std::cout << "  Question 1 started\n";
  EXPECT_EQ(WaitForEvent(*a, "question_started")["payload"]["question_index"].get<int>(), 1);
  EXPECT_EQ(WaitForEvent(*b, "question_started")["payload"]["question_index"].get<int>(), 1);

  auto a2 = a->SendCommand(
      {{"id", "5"},
       {"type", "submit_answer"},
       {"payload", {{"session_id", sid}, {"player_id", aid}, {"selected_indices", {1}}, {"time_ms", AnswerTimeFast}}}});
  EXPECT_TRUE(a2["success"].get<bool>());
  auto b2 = b->SendCommand(
      {{"id", "5"},
       {"type", "submit_answer"},
       {"payload", {{"session_id", sid}, {"player_id", bid}, {"selected_indices", {1}}, {"time_ms", AnswerTimeSlow}}}});
  EXPECT_TRUE(b2["success"].get<bool>());

  EXPECT_EQ(WaitForEvent(*h, "question_timeout")["event_type"], "question_timeout");
  auto finalReveal = WaitForEvent(*h, "answer_reveal");
  EXPECT_EQ(finalReveal["event_type"], "answer_reveal");
  auto hfin = WaitForEvent(*h, "game_finished");
  EXPECT_EQ(hfin["event_type"], "game_finished");
  std::cout << "  Game finished! Final: ";
  for (const auto& e : hfin["payload"]["final_leaderboard"]) {
    std::cout << e["player_id"].get<std::string>() << "=" << e["score"].get<int>() << " ";
  }
  std::cout << "\n";

  h->Close();
  a->Close();
  b->Close();
}

// ---- 3. Wrong PIN rejected ----
TEST_F(NetworkTest, WrongPinRejected) {
  auto h = MakeClient();
  auto qr = h->SendCommand({{"id", "1"}, {"type", "create_quiz"}, {"payload", MakeQuizPayload("Q")}});
  std::string qc = qr["payload"]["quiz_code"];
  h->SendCommand({{"id", "2"}, {"type", "create_session"}, {"payload", {{"quiz_code", qc}, {"host_name", "Host"}}}});

  auto p = MakeClient();
  auto jr = p->SendCommand({{"id", "1"}, {"type", "join"}, {"payload", {{"pin", "000000"}, {"name", "Eve"}}}});
  EXPECT_FALSE(jr["success"].get<bool>());
  std::cout << "  Wrong PIN correctly rejected\n";
  h->Close();
  p->Close();
}

// ---- 4. Player reconnection ----
TEST_F(NetworkTest, PlayerReconnection) {
  auto h = MakeClient();
  auto qr = h->SendCommand({{"id", "1"}, {"type", "create_quiz"}, {"payload", MakeQuizPayload("Q")}});
  std::string qc = qr["payload"]["quiz_code"];
  auto sr = h->SendCommand(
      {{"id", "2"}, {"type", "create_session"}, {"payload", {{"quiz_code", qc}, {"host_name", "Host"}}}});
  std::string sid = sr["payload"]["session_id"];
  std::string pin = sr["payload"]["pin"];

  // Player joins
  auto p = MakeClient();
  auto jr = p->SendCommand({{"id", "1"}, {"type", "join"}, {"payload", {{"pin", pin}, {"name", "Alice"}}}});
  ASSERT_TRUE(jr["success"].get<bool>());
  std::string pid = jr["payload"]["player_id"];
  std::cout << "  Player joined: " << pid << "\n";
  WaitForEvent(*h, "player_joined"); // consume PlayerJoined

  // Drop connection
  p->Drop();
  std::this_thread::sleep_for(std::chrono::milliseconds(TimerIntervalMs));
  std::cout << "  Player disconnected\n";

  // Verify disconnected but still in session
  auto session = session_manager_->GetSessionById(sid);
  ASSERT_TRUE(session.has_value());
  bool found = false;
  for (const auto& pl : session.value().players) { // NOLINT(bugprone-unchecked-optional-access)
    if (pl.id == pid) {
      EXPECT_FALSE(pl.connected);
      found = true;
    }
  }
  EXPECT_TRUE(found) << "Player should still exist in session";
  std::cout << "  Player still in session (disconnected)\n";

  // Reconnect
  auto p2 = MakeClient();
  auto rr =
      p2->SendCommand({{"id", "1"}, {"type", "reconnect"}, {"payload", {{"session_id", sid}, {"player_id", pid}}}});
  EXPECT_TRUE(rr["success"].get<bool>());
  std::cout << "  Player reconnected successfully\n";

  session = session_manager_->GetSessionById(sid);
  ASSERT_TRUE(session.has_value());
  for (const auto& pl : session.value().players) { // NOLINT(bugprone-unchecked-optional-access)
    if (pl.id == pid) {
      EXPECT_TRUE(pl.connected);
    }
  }

  h->Close();
  p2->Close();
}

// ---- 5. Max players (50) enforced ----
TEST_F(NetworkTest, MaxPlayersRejection) {
  auto h = MakeClient();
  auto qr = h->SendCommand({{"id", "1"}, {"type", "create_quiz"}, {"payload", MakeQuizPayload("Q")}});
  std::string qc = qr["payload"]["quiz_code"];
  auto sr = h->SendCommand(
      {{"id", "2"}, {"type", "create_session"}, {"payload", {{"quiz_code", qc}, {"host_name", "Host"}}}});
  std::string pin = sr["payload"]["pin"];

  // Join 49 players (host = #1, so 49 more → 50 total)
  std::vector<ClientPtr> players;
  for (int i = 0; i < MaxExtraPlayers; ++i) {
    auto c = MakeClient();
    auto resp = c->SendCommand(
        {{"id", "1"}, {"type", "join"}, {"payload", {{"pin", pin}, {"name", "P" + std::to_string(i + 1)}}}});
    ASSERT_TRUE(resp["success"].get<bool>()) << "Player " << i << " should join";
    players.push_back(std::move(c));
  }
  std::cout << "  50 players in session (host + 49)\n";

  // 51st rejected
  auto extra = MakeClient();
  auto resp = extra->SendCommand({{"id", "1"}, {"type", "join"}, {"payload", {{"pin", pin}, {"name", "Overflow"}}}});
  EXPECT_FALSE(resp["success"].get<bool>());
  std::cout << "  51st player correctly rejected\n";

  h->Close();
  extra->Close();
  for (auto& c : players)
    c->Close();
}

// ---- 6. Duplicate answer rejected ----
TEST_F(NetworkTest, DuplicateAnswerRejected) {
  auto h = MakeClient();
  auto qr = h->SendCommand({{"id", "1"}, {"type", "create_quiz"}, {"payload", MakeQuizPayload("Q")}});
  std::string qc = qr["payload"]["quiz_code"];
  auto sr = h->SendCommand(
      {{"id", "2"}, {"type", "create_session"}, {"payload", {{"quiz_code", qc}, {"host_name", "Host"}}}});
  std::string sid = sr["payload"]["session_id"];
  std::string pin = sr["payload"]["pin"];

  auto p = MakeClient();
  auto jr = p->SendCommand({{"id", "1"}, {"type", "join"}, {"payload", {{"pin", pin}, {"name", "Alice"}}}});
  std::string pid = jr["payload"]["player_id"];

  h->SendCommand({{"id", "3"}, {"type", "start_game"}, {"payload", {{"session_id", sid}}}});
  WaitForEvent(*p, "question_started");

  // First answer OK
  auto a1 = p->SendCommand(
      {{"id", "2"},
       {"type", "submit_answer"},
       {"payload", {{"session_id", sid}, {"player_id", pid}, {"selected_indices", {1}}, {"time_ms", AnswerTimeFast}}}});
  EXPECT_TRUE(a1["success"].get<bool>());
  std::cout << "  First answer accepted\n";

  // Second answer rejected
  auto a2 = p->SendCommand(
      {{"id", "3"},
       {"type", "submit_answer"},
       {"payload",
        {{"session_id", sid}, {"player_id", pid}, {"selected_indices", {0}}, {"time_ms", AnswerTimeNormal}}}});
  EXPECT_FALSE(a2["success"].get<bool>());
  std::cout << "  Duplicate answer correctly rejected\n";

  h->Close();
  p->Close();
}

// ---- 7. Two parallel games ----
TEST_F(NetworkTest, TwoParallelGames) {
  // Both games share the same server — tests per-session mutex isolation

  auto play_game = [this](const std::string& game_name, const std::string& host_name) {
    auto h = MakeClient();

    // Create quiz
    auto qr = h->SendCommand({{"id", "1"}, {"type", "create_quiz"}, {"payload", MakeQuizPayload(game_name)}});
    ASSERT_TRUE(qr["success"].get<bool>());
    std::string qc = qr["payload"]["quiz_code"];

    // Create session
    auto sr = h->SendCommand(
        {{"id", "2"}, {"type", "create_session"}, {"payload", {{"quiz_code", qc}, {"host_name", host_name}}}});
    ASSERT_TRUE(sr["success"].get<bool>());
    std::string sid = sr["payload"]["session_id"];
    std::string pin = sr["payload"]["pin"];
    std::cout << "  [" << game_name << "] Session " << sid << " (pin=" << pin << ")\n";

    // Two players join
    auto p1 = MakeClient();
    auto jr1 = p1->SendCommand({{"id", "1"}, {"type", "join"}, {"payload", {{"pin", pin}, {"name", "P1"}}}});
    ASSERT_TRUE(jr1["success"].get<bool>());
    std::string pid1 = jr1["payload"]["player_id"];
    WaitForEvent(*h, "player_joined");

    auto p2 = MakeClient();
    auto jr2 = p2->SendCommand({{"id", "1"}, {"type", "join"}, {"payload", {{"pin", pin}, {"name", "P2"}}}});
    ASSERT_TRUE(jr2["success"].get<bool>());
    std::string pid2 = jr2["payload"]["player_id"];
    WaitForEvent(*h, "player_joined");
    WaitForEvent(*p1, "player_joined");

    std::cout << "  [" << game_name << "] Players: " << pid1 << ", " << pid2 << "\n";

    // Start game
    auto sgr = h->SendCommand({{"id", "3"}, {"type", "start_game"}, {"payload", {{"session_id", sid}}}});
    ASSERT_TRUE(sgr["success"].get<bool>());
    WaitForEvent(*h, "question_started");
    WaitForEvent(*p1, "question_started");
    WaitForEvent(*p2, "question_started");
    std::cout << "  [" << game_name << "] Game started\n";

    // Both players answer question 0
    auto a1 = p1->SendCommand(
        {{"id", "2"},
         {"type", "submit_answer"},
         {"payload",
          {{"session_id", sid}, {"player_id", pid1}, {"selected_indices", {1}}, {"time_ms", AnswerTimeMedium}}}});
    EXPECT_TRUE(a1["success"].get<bool>());

    auto a2 = p2->SendCommand(
        {{"id", "2"},
         {"type", "submit_answer"},
         {"payload",
          {{"session_id", sid}, {"player_id", pid2}, {"selected_indices", {1}}, {"time_ms", AnswerTimeSlow}}}});
    EXPECT_TRUE(a2["success"].get<bool>());
    std::cout << "  [" << game_name << "] Answers submitted for Q0\n";

    // Next question
    h->SendCommand({{"id", "4"}, {"type", "next_question"}, {"payload", {{"session_id", sid}}}});
    WaitForEvent(*h, "question_started");
    WaitForEvent(*p1, "question_started");
    WaitForEvent(*p2, "question_started");

    auto q2a1 = p1->SendCommand(
        {{"id", "5"},
         {"type", "submit_answer"},
         {"payload",
          {{"session_id", sid}, {"player_id", pid1}, {"selected_indices", {1}}, {"time_ms", AnswerTimeMedium}}}});
    EXPECT_TRUE(q2a1["success"].get<bool>());
    auto q2a2 = p2->SendCommand(
        {{"id", "5"},
         {"type", "submit_answer"},
         {"payload",
          {{"session_id", sid}, {"player_id", pid2}, {"selected_indices", {1}}, {"time_ms", AnswerTimeSlow}}}});
    EXPECT_TRUE(q2a2["success"].get<bool>());

    // Finish
    auto fr = h->SendCommand({{"id", "6"}, {"type", "next_question"}, {"payload", {{"session_id", sid}}}});
    EXPECT_TRUE(fr["success"].get<bool>());
    auto fin = WaitForEvent(*h, "game_finished");
    EXPECT_EQ(fin["event_type"], "game_finished");

    std::cout << "  [" << game_name << "] Game finished! Scores: ";
    for (const auto& e : fin["payload"]["final_leaderboard"]) {
      std::cout << e["player_id"].get<std::string>() << "=" << e["score"].get<int>() << " ";
    }
    std::cout << "\n";

    h->Close();
    p1->Close();
    p2->Close();
  };

  // Run two games in parallel threads
  std::thread game1([&]() { play_game("Game-A", "hostA"); });
  std::thread game2([&]() { play_game("Game-B", "hostB"); });

  game1.join();
  game2.join();
  std::cout << "  Both games completed in parallel\n";
}

} // namespace quizlyx::server
