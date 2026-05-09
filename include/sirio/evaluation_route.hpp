#pragma once

#include <cstdint>
#include <string>

#include "sirio/board.hpp"
#include "sirio/nnue/backend.hpp"

namespace sirio {

enum class EvaluationRoute {
    DefaultExisting = 0,
    ExperimentalSirioNNUE2 = 1,
};

struct EvaluationRouteResult {
    std::int32_t score = 0;
    bool used_default_route = true;
    bool used_experimental_route = false;
    bool fell_back_to_default = false;
};

[[nodiscard]] EvaluationRouteResult evaluate_with_experimental_backend_for_tests(
    const Board &board, std::int32_t default_score, EvaluationRoute route,
    const nnue::Nnue2NetworkParameters *network, std::string *diagnostic_message = nullptr);

[[nodiscard]] EvaluationRouteResult evaluate_with_experimental_backend_for_tests(
    const Board &board, EvaluationRoute route, const nnue::Nnue2NetworkParameters *network,
    std::string *diagnostic_message = nullptr);

}  // namespace sirio
