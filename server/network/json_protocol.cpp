#include "json_protocol.hpp"

namespace quizlyx::server::network {

std::optional<ClientMessage> ParseClientMessage(const std::string& text) {
  try {
    auto j = nlohmann::json::parse(text);
    ClientMessage msg;
    msg.id = j.value("id", "");
    msg.type = j.at("type").get<std::string>();
    msg.payload = j.value("payload", nlohmann::json::object());
    return msg;
  } catch (...) {
    return std::nullopt;
  }
}

std::string SerializeResponse(const std::string& request_id, bool success, const nlohmann::json& payload) {
  nlohmann::json j;
  j["id"] = request_id;
  j["type"] = "response";
  j["success"] = success;
  j["payload"] = payload;
  return j.dump();
}

std::string SerializeGameEvent(const events::GameEvent& event, const std::string& viewer_player_id) {
  nlohmann::json j;
  j["type"] = "event";

  std::visit(
      [&j, &viewer_player_id](auto&& ev) {
        using T = std::decay_t<decltype(ev)>;
        if constexpr (std::is_same_v<T, events::QuestionStarted>) {
          j["event_type"] = "question_started";
          j["payload"] = {{"question_index", ev.question_index},
                          {"total_questions", ev.total_questions},
                          {"duration_ms", ev.duration_ms.count()},
                          {"text", ev.text},
                          {"answer_type",
                           ev.answer_type == domain::AnswerType::MultipleChoice ? "multiple_choice" : "single_choice"},
                          {"options", ev.options}};
        } else if constexpr (std::is_same_v<T, events::TimerUpdate>) {
          j["event_type"] = "timer_update";
          j["payload"] = {{"remaining_ms", ev.remaining_ms.count()}};
        } else if constexpr (std::is_same_v<T, events::QuestionTimeout>) {
          j["event_type"] = "question_timeout";
          j["payload"] = nlohmann::json::object();
        } else if constexpr (std::is_same_v<T, events::AnswerReveal>) {
          j["event_type"] = "answer_reveal";
          const auto selection_it = ev.selections_by_player.find(viewer_player_id);
          const auto& my_selected_indices =
              selection_it != ev.selections_by_player.end() ? selection_it->second : std::vector<size_t>{};
          j["payload"] = {{"correct_indices", ev.correct_indices},
                          {"my_selected_indices", my_selected_indices},
                          {"option_pick_counts", ev.option_pick_counts},
                          {"reveal_duration_ms", ev.reveal_duration_ms.count()}};
        } else if constexpr (std::is_same_v<T, events::Leaderboard>) {
          j["event_type"] = "leaderboard";
          nlohmann::json entries = nlohmann::json::array();
          for (const auto& entry : ev.entries) {
            entries.push_back({{"player_id", entry.player_id}, {"score", entry.score}});
          }
          j["payload"] = {{"entries", entries}};
          if (ev.next_round_delay_ms.has_value()) {
            j["payload"]["next_round_delay_ms"] = ev.next_round_delay_ms->count();
          }
        } else if constexpr (std::is_same_v<T, events::PlayerJoined>) {
          j["event_type"] = "player_joined";
          j["payload"] = {{"player_id", ev.player_id},
                          {"display_name", ev.display_name},
                          {"role", ev.role == domain::Role::Host ? "host" : "player"},
                          {"is_competing", ev.is_competing}};
        } else if constexpr (std::is_same_v<T, events::PlayerLeft>) {
          j["event_type"] = "player_left";
          j["payload"] = {{"player_id", ev.player_id}};
        } else if constexpr (std::is_same_v<T, events::GameFinished>) {
          j["event_type"] = "game_finished";
          nlohmann::json entries = nlohmann::json::array();
          for (const auto& entry : ev.final_leaderboard) {
            entries.push_back({{"player_id", entry.player_id}, {"score", entry.score}});
          }
          j["payload"] = {{"final_leaderboard", entries}};
        }
      },
      event);

  return j.dump();
}

domain::Quiz DeserializeQuiz(const nlohmann::json& j) {
  domain::Quiz quiz;
  quiz.title = j.at("title").get<std::string>();
  quiz.description = j.value("description", "");

  for (const auto& qj : j.at("questions")) {
    domain::Question q;
    q.text = qj.at("text").get<std::string>();

    std::string answer_type_str = qj.value("answer_type", "single_choice");
    q.answer_type =
        (answer_type_str == "multiple_choice") ? domain::AnswerType::MultipleChoice : domain::AnswerType::SingleChoice;

    q.options = qj.at("options").get<std::vector<std::string>>();
    q.correct_indices = qj.at("correct_indices").get<std::vector<size_t>>();
    q.time_limit_ms = std::chrono::milliseconds(qj.at("time_limit_ms").get<int>());

    quiz.questions.push_back(std::move(q));
  }

  return quiz;
}

domain::PlayerAnswer DeserializePlayerAnswer(const nlohmann::json& j) {
  domain::PlayerAnswer answer;
  answer.selected_indices = j.at("selected_indices").get<std::vector<size_t>>();
  answer.time_since_question_start_ms = std::chrono::milliseconds(j.value("time_ms", 0));
  return answer;
}

} // namespace quizlyx::server::network
