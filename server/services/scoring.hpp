#ifndef QUIZLYX_SERVER_SERVICES_SCORING_HPP
#define QUIZLYX_SERVER_SERVICES_SCORING_HPP

#include "domain/answer.hpp"
#include "domain/quiz.hpp"

namespace quizlyx::server::services {

int CalculatePoints(const domain::Question& question, const domain::PlayerAnswer& answer);

} // namespace quizlyx::server::services

#endif // QUIZLYX_SERVER_SERVICES_SCORING_HPP
