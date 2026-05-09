#include "sirio/evaluation_route.hpp"

#include "sirio/evaluation.hpp"

namespace sirio {

namespace {

EvaluationRouteResult build_route_result(std::int32_t score, EvaluationRoute selected_route,
                                         const nnue::ExperimentalEvalRoutingResult &routed) {
    EvaluationRouteResult result{};
    result.score = score;
    result.selected_route = selected_route;
    result.used_experimental_route = routed.used_experimental_backend;
    result.fell_back_to_default = routed.fell_back_to_classical;
    result.used_default_route = !result.used_experimental_route;
    result.actual_route =
        result.used_experimental_route ? EvaluationRoute::ExperimentalSirioNNUE2
                                       : EvaluationRoute::DefaultExisting;
    return result;
}

}  // namespace


ExperimentalEvaluationState prepare_experimental_evaluation_state_for_tests(
    const ExperimentalEvaluationConfig &config) {
    ExperimentalEvaluationState state{};
    state.config = config;
    if (config.selected_route == EvaluationRoute::DefaultExisting) {
        return state;
    }

    state.load_attempted = true;
    if (!config.network_path.has_value() || config.network_path->empty()) {
        state.load_status = ExperimentalEvaluationLoadStatus::LoadRejected;
        state.fallback_reason =
            "network load rejected: Experimental SirioNNUE2 route requires network file path";
        return state;
    }

    nnue::Nnue2NetworkParameters network;
    std::string load_error;
    if (!nnue::load_nnue2_network_file(*config.network_path, network, load_error)) {
        state.load_status = ExperimentalEvaluationLoadStatus::LoadRejected;
        state.fallback_reason = "network load rejected: " + load_error;
        return state;
    }

    state.load_status = ExperimentalEvaluationLoadStatus::Loaded;
    state.load_succeeded = true;
    state.loaded_network = std::move(network);
    return state;
}

EvaluationRouteResult evaluate_with_experimental_evaluation_state_for_tests(
    const Board &board, std::int32_t default_score, const ExperimentalEvaluationState &state,
    std::string *diagnostic_message) {
    if (state.config.selected_route == EvaluationRoute::DefaultExisting) {
        EvaluationRouteResult result{};
        result.score = default_score;
        result.selected_route = state.config.selected_route;
        result.actual_route = EvaluationRoute::DefaultExisting;
        if (diagnostic_message) {
            *diagnostic_message = "Experimental backend disabled; classical evaluation preserved";
        }
        return result;
    }

    EvaluationRouteResult result{};
    result.selected_route = state.config.selected_route;
    result.attempted_file_load = state.load_attempted;
    result.file_load_succeeded = state.load_succeeded;

    if (!state.loaded_network.has_value()) {
        result.score = default_score;
        result.actual_route = EvaluationRoute::DefaultExisting;
        result.used_default_route = true;
        result.fell_back_to_default = true;
        result.fallback_reason = state.fallback_reason.empty()
                                     ? "network load rejected: unavailable experimental state"
                                     : state.fallback_reason;
        if (diagnostic_message) {
            *diagnostic_message =
                "Experimental SirioNNUE2 backend selected but state not loaded: " +
                result.fallback_reason + "; using classical fallback";
        }
        return result;
    }

    std::string route_diag;
    const auto routed = nnue::route_experimental_nnue2_evaluation(
        board, nnue::ExperimentalEvalBackend::ExperimentalSirioNNUE2, default_score,
        &(*state.loaded_network), &route_diag);
    result = build_route_result(routed.score, state.config.selected_route, routed);
    result.attempted_file_load = state.load_attempted;
    result.file_load_succeeded = state.load_succeeded;
    if (routed.fell_back_to_classical) {
        result.fallback_reason = route_diag;
    }
    if (diagnostic_message) {
        *diagnostic_message = route_diag;
    }
    return result;
}

EvaluationRouteResult evaluate_with_experimental_evaluation_state_for_tests(
    const Board &board, const ExperimentalEvaluationState &state, std::string *diagnostic_message) {
    const std::int32_t default_score = evaluate(board);
    return evaluate_with_experimental_evaluation_state_for_tests(board, default_score, state,
                                                                 diagnostic_message);
}

EvaluationRouteResult evaluate_with_experimental_backend_for_tests(
    const Board &board, std::int32_t default_score, EvaluationRoute route,
    const nnue::Nnue2NetworkParameters *network, std::string *diagnostic_message) {
    nnue::ExperimentalEvalBackend backend = nnue::ExperimentalEvalBackend::Classical;
    if (route == EvaluationRoute::ExperimentalSirioNNUE2) {
        backend = nnue::ExperimentalEvalBackend::ExperimentalSirioNNUE2;
    }

    const auto routed = nnue::route_experimental_nnue2_evaluation(
        board, backend, default_score, network, diagnostic_message);
    return build_route_result(routed.score, route, routed);
}

EvaluationRouteResult evaluate_with_experimental_backend_for_tests(
    const Board &board, EvaluationRoute route, const nnue::Nnue2NetworkParameters *network,
    std::string *diagnostic_message) {
    const std::int32_t default_score = evaluate(board);
    return evaluate_with_experimental_backend_for_tests(board, default_score, route, network,
                                                        diagnostic_message);
}

EvaluationRouteResult evaluate_with_experimental_backend_file_for_tests(
    const Board &board, std::int32_t default_score, EvaluationRoute route,
    const std::string &network_path, std::string *diagnostic_message) {
    if (route == EvaluationRoute::DefaultExisting) {
        EvaluationRouteResult result{};
        result.score = default_score;
        result.selected_route = route;
        result.actual_route = EvaluationRoute::DefaultExisting;
        if (diagnostic_message) {
            *diagnostic_message = "Experimental backend disabled; classical evaluation preserved";
        }
        return result;
    }

    EvaluationRouteResult result{};
    result.selected_route = route;
    result.attempted_file_load = true;

    nnue::Nnue2NetworkParameters network;
    std::string load_error;
    if (!nnue::load_nnue2_network_file(network_path, network, load_error)) {
        result.score = default_score;
        result.actual_route = EvaluationRoute::DefaultExisting;
        result.used_default_route = true;
        result.fell_back_to_default = true;
        result.file_load_succeeded = false;
        result.fallback_reason = "network load rejected: " + load_error;
        if (diagnostic_message) {
            *diagnostic_message = "Experimental SirioNNUE2 backend selected but file load failed: " +
                                  load_error + "; using classical fallback";
        }
        return result;
    }

    result.file_load_succeeded = true;
    std::string route_diag;
    const auto routed = nnue::route_experimental_nnue2_evaluation(
        board, nnue::ExperimentalEvalBackend::ExperimentalSirioNNUE2, default_score, &network,
        &route_diag);
    result = build_route_result(routed.score, route, routed);
    result.attempted_file_load = true;
    result.file_load_succeeded = true;
    if (routed.fell_back_to_classical) {
        result.fallback_reason = route_diag;
    }
    if (diagnostic_message) {
        *diagnostic_message = route_diag;
    }
    return result;
}

EvaluationRouteResult evaluate_with_experimental_backend_file_for_tests(
    const Board &board, EvaluationRoute route, const std::string &network_path,
    std::string *diagnostic_message) {
    const std::int32_t default_score = evaluate(board);
    return evaluate_with_experimental_backend_file_for_tests(board, default_score, route,
                                                             network_path, diagnostic_message);
}

}  // namespace sirio
