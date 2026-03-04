#ifndef QUIZLYX_SERVER_DOMAIN_TYPES_HPP
#define QUIZLYX_SERVER_DOMAIN_TYPES_HPP

namespace quizlyx::server::domain {

enum class SessionState { Lobby, Running, Finished };

enum class Role { Host, Player };

enum class AnswerType { SingleChoice, MultipleChoice };

}  // namespace quizlyx::server::domain

#endif  // QUIZLYX_SERVER_DOMAIN_TYPES_HPP
