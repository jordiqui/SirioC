#include "sirio/evaluation_route.hpp"

#include "sirio/evaluation.hpp"

namespace sirio {

EvaluationRouteResult evaluate_with_experimental_backend_for_tests(
    const Board &board, std::int32_t default_score, EvaluationRoute route,
    const nnue::Nnue2NetworkParameters *network, std::string *diagnostic_message) {
    nnue::ExperimentalEvalBackend backend = nnue::ExperimentalEvalBackend::Classical;
    if (route == EvaluationRoute::ExperimentalSirioNNUE2) {
        backend = nnue::ExperimentalEvalBackend::ExperimentalSirioNNUE2;
    }

    const auto routed = nnue::route_experimental_nnue2_evaluation(
        board, backend, default_score, network, diagnostic_message);

    EvaluationRouteResult result{};
    result.score = routed.score;
    result.used_experimental_route = routed.used_experimental_backend;
    result.fell_back_to_default = routed.fell_back_to_classical;
    result.used_default_route = !result.used_experimental_route;
    return result;
}

EvaluationRouteResult evaluate_with_experimental_backend_for_tests(
    const Board &board, EvaluationRoute route, const nnue::Nnue2NetworkParameters *network,
    std::string *diagnostic_message) {
    const std::int32_t default_score = evaluate(board);
    return evaluate_with_experimental_backend_for_tests(board, default_score, route, network,
                                                        diagnostic_message);
}

}  // namespace sirio
