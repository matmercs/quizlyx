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

  void Close() { ws_.close(websocket::close_code::normal); }
  void Drop() { beast::get_lowest_layer(ws_).close(); }

private:
  json ReadRaw() {
    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(5));
    beast::flat_buffer buf;
    ws_.read(buf);
    return json::parse(beast::buffers_to_string(buf.data()));
  }

  net::io_context ioc_;
  websocket::stream<beast::tcp_stream> ws_{ioc_};
  std::queue<json> event_queue_;
};

using ClientPtr = std::unique_ptr<TestClient>;

static json MakeQuizPayload(const std::string& title, int time_limit_ms = 5000) {
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
    session_manager_ =
        std::make_unique<services::SessionManager>(*quiz_registry_, *broadcast_sink_, *time_provider_);
    timer_service_ =
        std::make_unique<services::SessionTimerService>(*time_provider_, std::chrono::milliseconds{200});
    command_handler_ = std::make_unique<app::ServerCommandHandler>(*quiz_registry_, *session_manager_);
    game_controller_ = std::make_unique<network::GameController>(*command_handler_, *timer_service_,
                                                                  *session_manager_);
    ioc_ = std::make_unique<net::io_context>();
    auto endpoint = tcp::endpoint(tcp::v4(), 0);
    server_ = std::make_unique<network::WsServer>(*ioc_, endpoint, *game_controller_, *connection_manager_);
    port_ = server_->Port();
    server_->Start();
    server_thread_ = std::thread([this]() { ioc_->run(); });
  }

  void TearDown() override {
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

  uint16_t port_ = 0;
  std::unique_ptr<net::io_context> ioc_;
  std::thread server_thread_;
  std::unique_ptr<services::InMemoryQuizStorage> quiz_storage_;
  std::unique_ptr<services::QuizRegistry> quiz_registry_;
  std::unique_ptr<TestTimeProvider> time_provider_;
  std::unique_ptr<network::WsConnectionManager> connection_manager_;
  std::unique_ptr<network::WsBroadcastSink> broadcast_sink_;
  std::unique_ptr<services::SessionManager> session_manager_;
  std::unique_ptr<services::SessionTimerService> timer_service_;
  std::unique_ptr<app::ServerCommandHandler> command_handler_;
  std::unique_ptr<network::GameController> game_controller_;
  std::unique_ptr<network::WsServer> server_;
};

// ---- 1. Create quiz and session ----
TEST_F(NetworkTest, CreateQuizAndSession) {
  auto h = MakeClient();
  auto qr = h->SendCommand({{"id", "1"}, {"type", "create_quiz"}, {"payload", MakeQuizPayload("Math")}});
  EXPECT_TRUE(qr["success"].get<bool>());
  std::string qc = qr["payload"]["quiz_code"];
  EXPECT_FALSE(qc.empty());
  std::cout << "  Quiz created: " << qc << "\n";

  auto sr = h->SendCommand(
      {{"id", "2"}, {"type", "create_session"}, {"payload", {{"quiz_code", qc}, {"host_id", "host1"}}}});
  EXPECT_TRUE(sr["success"].get<bool>());
  std::string sid = sr["payload"]["session_id"];
  std::string pin = sr["payload"]["pin"];
  EXPECT_FALSE(sid.empty());
  EXPECT_EQ(pin.size(), 6u);
  std::cout << "  Session: id=" << sid << " pin=" << pin << "\n";
  h->Close();
}

// ---- 2. Full game flow ----
TEST_F(NetworkTest, FullGameFlow) {
  auto h = MakeClient();

  auto qr = h->SendCommand({{"id", "1"}, {"type", "create_quiz"}, {"payload", MakeQuizPayload("Demo")}});
  ASSERT_TRUE(qr["success"].get<bool>());
  std::string qc = qr["payload"]["quiz_code"];

  auto sr = h->SendCommand(
      {{"id", "2"}, {"type", "create_session"}, {"payload", {{"quiz_code", qc}, {"host_id", "host1"}}}});
  ASSERT_TRUE(sr["success"].get<bool>());
  std::string sid = sr["payload"]["session_id"];
  std::string pin = sr["payload"]["pin"];
  std::cout << "  Session " << sid << " (pin=" << pin << ")\n";

  // Alice joins
  auto a = MakeClient();
  auto ar = a->SendCommand(
      {{"id", "1"}, {"type", "join"}, {"payload", {{"session_id", sid}, {"pin", pin}, {"name", "Alice"}}}});
  ASSERT_TRUE(ar["success"].get<bool>());
  std::string aid = ar["payload"]["player_id"];
  std::cout << "  Alice joined: " << aid << "\n";

  // Host gets PlayerJoined for Alice
  auto he = h->ReadEvent();
  EXPECT_EQ(he["event_type"], "player_joined");
  EXPECT_EQ(he["payload"]["player_id"], aid);

  // Bob joins
  auto b = MakeClient();
  auto br = b->SendCommand(
      {{"id", "1"}, {"type", "join"}, {"payload", {{"session_id", sid}, {"pin", pin}, {"name", "Bob"}}}});
  ASSERT_TRUE(br["success"].get<bool>());
  std::string bid = br["payload"]["player_id"];
  std::cout << "  Bob joined: " << bid << "\n";

  // Host and Alice get PlayerJoined for Bob
  auto he2 = h->ReadEvent();
  EXPECT_EQ(he2["event_type"], "player_joined");
  auto ae = a->ReadEvent();
  EXPECT_EQ(ae["event_type"], "player_joined");

  // Start game
  auto sgr = h->SendCommand({{"id", "3"}, {"type", "start_game"}, {"payload", {{"session_id", sid}}}});
  EXPECT_TRUE(sgr["success"].get<bool>());
  std::cout << "  Game started\n";

  // All get QuestionStarted
  auto hqs = h->ReadEvent();
  EXPECT_EQ(hqs["event_type"], "question_started");
  EXPECT_EQ(hqs["payload"]["question_index"], 0);
  std::cout << "  Question 0 started (duration=" << hqs["payload"]["duration_ms"] << "ms)\n";
  auto aqs = a->ReadEvent();
  EXPECT_EQ(aqs["event_type"], "question_started");
  auto bqs = b->ReadEvent();
  EXPECT_EQ(bqs["event_type"], "question_started");

  // Alice answers correctly (index 1)
  auto aa = a->SendCommand(
      {{"id", "2"},
       {"type", "submit_answer"},
       {"payload", {{"session_id", sid}, {"player_id", aid}, {"selected_indices", {1}}, {"time_ms", 200}}}});
  EXPECT_TRUE(aa["success"].get<bool>());
  std::cout << "  Alice answered (correct)\n";

  auto hlb1 = h->ReadEvent();
  EXPECT_EQ(hlb1["event_type"], "leaderboard");

  // Bob answers wrong (index 0)
  auto ba = b->SendCommand(
      {{"id", "2"},
       {"type", "submit_answer"},
       {"payload", {{"session_id", sid}, {"player_id", bid}, {"selected_indices", {0}}, {"time_ms", 500}}}});
  EXPECT_TRUE(ba["success"].get<bool>());
  std::cout << "  Bob answered (wrong)\n";

  auto hlb2 = h->ReadEvent();
  EXPECT_EQ(hlb2["event_type"], "leaderboard");
  std::cout << "  Leaderboard: ";
  for (const auto& e : hlb2["payload"]["entries"]) {
    std::cout << e["player_id"].get<std::string>() << "=" << e["score"].get<int>() << " ";
  }
  std::cout << "\n";

  // Next question
  auto nr = h->SendCommand({{"id", "4"}, {"type", "next_question"}, {"payload", {{"session_id", sid}}}});
  EXPECT_TRUE(nr["success"].get<bool>());
  auto hqs2 = h->ReadEvent();
  EXPECT_EQ(hqs2["event_type"], "question_started");
  EXPECT_EQ(hqs2["payload"]["question_index"], 1);
  std::cout << "  Question 1 started\n";

  // Advance to finish
  auto fr = h->SendCommand({{"id", "5"}, {"type", "next_question"}, {"payload", {{"session_id", sid}}}});
  EXPECT_TRUE(fr["success"].get<bool>());
  auto hfin = h->ReadEvent();
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
  auto sr = h->SendCommand(
      {{"id", "2"}, {"type", "create_session"}, {"payload", {{"quiz_code", qc}, {"host_id", "host1"}}}});
  std::string sid = sr["payload"]["session_id"];

  auto p = MakeClient();
  auto jr = p->SendCommand(
      {{"id", "1"}, {"type", "join"}, {"payload", {{"session_id", sid}, {"pin", "000000"}}}});
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
      {{"id", "2"}, {"type", "create_session"}, {"payload", {{"quiz_code", qc}, {"host_id", "host1"}}}});
  std::string sid = sr["payload"]["session_id"];
  std::string pin = sr["payload"]["pin"];

  // Player joins
  auto p = MakeClient();
  auto jr = p->SendCommand(
      {{"id", "1"}, {"type", "join"}, {"payload", {{"session_id", sid}, {"pin", pin}}}});
  ASSERT_TRUE(jr["success"].get<bool>());
  std::string pid = jr["payload"]["player_id"];
  std::cout << "  Player joined: " << pid << "\n";
  h->ReadEvent(); // consume PlayerJoined

  // Drop connection
  p->Drop();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  std::cout << "  Player disconnected\n";

  // Verify disconnected but still in session
  auto session = session_manager_->GetSessionById(sid);
  ASSERT_TRUE(session.has_value());
  bool found = false;
  for (const auto& pl : session->players) {
    if (pl.id == pid) {
      EXPECT_FALSE(pl.connected);
      found = true;
    }
  }
  EXPECT_TRUE(found) << "Player should still exist in session";
  std::cout << "  Player still in session (disconnected)\n";

  // Reconnect
  auto p2 = MakeClient();
  auto rr = p2->SendCommand(
      {{"id", "1"}, {"type", "reconnect"}, {"payload", {{"session_id", sid}, {"player_id", pid}}}});
  EXPECT_TRUE(rr["success"].get<bool>());
  std::cout << "  Player reconnected successfully\n";

  session = session_manager_->GetSessionById(sid);
  for (const auto& pl : session->players) {
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
      {{"id", "2"}, {"type", "create_session"}, {"payload", {{"quiz_code", qc}, {"host_id", "host1"}}}});
  std::string sid = sr["payload"]["session_id"];
  std::string pin = sr["payload"]["pin"];

  // Join 49 players (host = #1, so 49 more → 50 total)
  std::vector<ClientPtr> players;
  for (int i = 0; i < 49; ++i) {
    auto c = MakeClient();
    auto resp = c->SendCommand(
        {{"id", "1"}, {"type", "join"}, {"payload", {{"session_id", sid}, {"pin", pin}}}});
    ASSERT_TRUE(resp["success"].get<bool>()) << "Player " << i << " should join";
    players.push_back(std::move(c));
  }
  std::cout << "  50 players in session (host + 49)\n";

  // 51st rejected
  auto extra = MakeClient();
  auto resp = extra->SendCommand(
      {{"id", "1"}, {"type", "join"}, {"payload", {{"session_id", sid}, {"pin", pin}}}});
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
      {{"id", "2"}, {"type", "create_session"}, {"payload", {{"quiz_code", qc}, {"host_id", "host1"}}}});
  std::string sid = sr["payload"]["session_id"];
  std::string pin = sr["payload"]["pin"];

  auto p = MakeClient();
  auto jr = p->SendCommand(
      {{"id", "1"}, {"type", "join"}, {"payload", {{"session_id", sid}, {"pin", pin}}}});
  std::string pid = jr["payload"]["player_id"];

  h->SendCommand({{"id", "3"}, {"type", "start_game"}, {"payload", {{"session_id", sid}}}});
  p->ReadEvent(); // QuestionStarted

  // First answer OK
  auto a1 = p->SendCommand(
      {{"id", "2"},
       {"type", "submit_answer"},
       {"payload", {{"session_id", sid}, {"player_id", pid}, {"selected_indices", {1}}, {"time_ms", 100}}}});
  EXPECT_TRUE(a1["success"].get<bool>());
  std::cout << "  First answer accepted\n";

  // Second answer rejected
  auto a2 = p->SendCommand(
      {{"id", "3"},
       {"type", "submit_answer"},
       {"payload", {{"session_id", sid}, {"player_id", pid}, {"selected_indices", {0}}, {"time_ms", 200}}}});
  EXPECT_FALSE(a2["success"].get<bool>());
  std::cout << "  Duplicate answer correctly rejected\n";

  h->Close();
  p->Close();
}

// ---- 7. Two parallel games ----
TEST_F(NetworkTest, TwoParallelGames) {
  // Both games share the same server — tests per-session mutex isolation

  auto play_game = [this](const std::string& game_name, const std::string& host_id) {
    auto h = MakeClient();

    // Create quiz
    auto qr = h->SendCommand({{"id", "1"}, {"type", "create_quiz"}, {"payload", MakeQuizPayload(game_name)}});
    ASSERT_TRUE(qr["success"].get<bool>());
    std::string qc = qr["payload"]["quiz_code"];

    // Create session
    auto sr = h->SendCommand(
        {{"id", "2"}, {"type", "create_session"}, {"payload", {{"quiz_code", qc}, {"host_id", host_id}}}});
    ASSERT_TRUE(sr["success"].get<bool>());
    std::string sid = sr["payload"]["session_id"];
    std::string pin = sr["payload"]["pin"];
    std::cout << "  [" << game_name << "] Session " << sid << " (pin=" << pin << ")\n";

    // Two players join
    auto p1 = MakeClient();
    auto jr1 = p1->SendCommand(
        {{"id", "1"}, {"type", "join"}, {"payload", {{"session_id", sid}, {"pin", pin}, {"name", "P1"}}}});
    ASSERT_TRUE(jr1["success"].get<bool>());
    std::string pid1 = jr1["payload"]["player_id"];
    h->ReadEvent(); // PlayerJoined

    auto p2 = MakeClient();
    auto jr2 = p2->SendCommand(
        {{"id", "1"}, {"type", "join"}, {"payload", {{"session_id", sid}, {"pin", pin}, {"name", "P2"}}}});
    ASSERT_TRUE(jr2["success"].get<bool>());
    std::string pid2 = jr2["payload"]["player_id"];
    h->ReadEvent();  // PlayerJoined
    p1->ReadEvent(); // PlayerJoined

    std::cout << "  [" << game_name << "] Players: " << pid1 << ", " << pid2 << "\n";

    // Start game
    auto sgr = h->SendCommand({{"id", "3"}, {"type", "start_game"}, {"payload", {{"session_id", sid}}}});
    ASSERT_TRUE(sgr["success"].get<bool>());
    h->ReadEvent();  // QuestionStarted
    p1->ReadEvent(); // QuestionStarted
    p2->ReadEvent(); // QuestionStarted
    std::cout << "  [" << game_name << "] Game started\n";

    // Both players answer question 0
    auto a1 = p1->SendCommand(
        {{"id", "2"},
         {"type", "submit_answer"},
         {"payload", {{"session_id", sid}, {"player_id", pid1}, {"selected_indices", {1}}, {"time_ms", 150}}}});
    EXPECT_TRUE(a1["success"].get<bool>());
    h->ReadEvent(); // Leaderboard

    auto a2 = p2->SendCommand(
        {{"id", "2"},
         {"type", "submit_answer"},
         {"payload", {{"session_id", sid}, {"player_id", pid2}, {"selected_indices", {1}}, {"time_ms", 300}}}});
    EXPECT_TRUE(a2["success"].get<bool>());
    h->ReadEvent(); // Leaderboard
    std::cout << "  [" << game_name << "] Answers submitted for Q0\n";

    // Next question
    h->SendCommand({{"id", "4"}, {"type", "next_question"}, {"payload", {{"session_id", sid}}}});
    h->ReadEvent(); // QuestionStarted

    // Finish
    auto fr = h->SendCommand({{"id", "5"}, {"type", "next_question"}, {"payload", {{"session_id", sid}}}});
    EXPECT_TRUE(fr["success"].get<bool>());
    auto fin = h->ReadEvent();
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
