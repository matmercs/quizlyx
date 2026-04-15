#ifndef QUIZLYX_SERVER_NETWORK_JSON_PROTOCOL_HPP
#define QUIZLYX_SERVER_NETWORK_JSON_PROTOCOL_HPP

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

#include "domain/answer.hpp"
#include "domain/quiz.hpp"
#include "events/game_events.hpp"

namespace quizlyx::server::network {

struct ClientMessage {
  std::string id;
  std::string type;
  nlohmann::json payload;
};

std::optional<ClientMessage> ParseClientMessage(const std::string& text);
std::string SerializeResponse(const std::string& request_id, bool success, const nlohmann::json& payload);
std::string SerializeGameEvent(const events::GameEvent& event, const std::string& viewer_player_id = {});

domain::Quiz DeserializeQuiz(const nlohmann::json& j);
domain::PlayerAnswer DeserializePlayerAnswer(const nlohmann::json& j);

} // namespace quizlyx::server::network

#endif // QUIZLYX_SERVER_NETWORK_JSON_PROTOCOL_HPP
