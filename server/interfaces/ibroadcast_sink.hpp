#ifndef QUIZLYX_SERVER_INTERFACES_IBROADCAST_SINK_HPP
#define QUIZLYX_SERVER_INTERFACES_IBROADCAST_SINK_HPP

#include <string>

#include "events/game_events.hpp"

namespace quizlyx::server::interfaces {

class IBroadcastSink {
 public:
  virtual ~IBroadcastSink() = default;

  virtual void Broadcast(const std::string& session_id, const events::GameEvent& event) = 0;
};

}  // namespace quizlyx::server::interfaces

#endif  // QUIZLYX_SERVER_INTERFACES_IBROADCAST_SINK_HPP
