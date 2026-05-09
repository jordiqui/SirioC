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



ExperimentalSirioNNUE2Runtime::ExperimentalSirioNNUE2Runtime(
    const ExperimentalEvaluationConfig &config) {
    load_from_config(config);
}

bool ExperimentalSirioNNUE2Runtime::load_from_config(const ExperimentalEvaluationConfig &config) {
    active_ = config.selected_route == EvaluationRoute::ExperimentalSirioNNUE2;
    loaded_network_.reset();
    fallback_reason_.clear();
    status_ = ExperimentalSirioNNUE2RuntimeStatus::Inactive;
    if (!active_) {
        return false;
    }
    if (!config.network_path.has_value() || config.network_path->empty()) {
        status_ = ExperimentalSirioNNUE2RuntimeStatus::LoadRejected;
        fallback_reason_ =
            "network load rejected: Experimental SirioNNUE2 route requires network file path";
        return false;
    }
    return load_from_file(*config.network_path);
}

bool ExperimentalSirioNNUE2Runtime::load_from_file(const std::string &network_path) {
    active_ = true;
    loaded_network_.reset();
    fallback_reason_.clear();
    nnue::Nnue2NetworkParameters network;
    std::string load_error;
    if (!nnue::load_nnue2_network_file(network_path, network, load_error)) {
        status_ = ExperimentalSirioNNUE2RuntimeStatus::LoadRejected;
        fallback_reason_ = "network load rejected: " + load_error;
        return false;
    }
    loaded_network_ = std::move(network);
    status_ = ExperimentalSirioNNUE2RuntimeStatus::Loaded;
    return true;
}

bool ExperimentalSirioNNUE2Runtime::is_active() const { return active_; }
bool ExperimentalSirioNNUE2Runtime::is_loaded() const { return loaded_network_.has_value(); }
ExperimentalSirioNNUE2RuntimeStatus ExperimentalSirioNNUE2Runtime::status() const { return status_; }
const std::string &ExperimentalSirioNNUE2Runtime::fallback_reason() const { return fallback_reason_; }
const nnue::Nnue2NetworkParameters *ExperimentalSirioNNUE2Runtime::loaded_network() const {
    return loaded_network_.has_value() ? &(*loaded_network_) : nullptr;
}

ExperimentalSirioNNUE2RuntimeResult ExperimentalSirioNNUE2Runtime::evaluate_with_fallback(
    const Board &board, std::int32_t default_score, std::string *diagnostic_message) const {
    ExperimentalSirioNNUE2RuntimeResult result{};
    result.score = default_score;
    result.status = status_;

    if (!active_ || !loaded_network_.has_value()) {
        result.fell_back_to_default = true;
        result.fallback_reason =
            fallback_reason_.empty() ? "experimental runtime inactive or unloaded" : fallback_reason_;
        if (diagnostic_message) {
            *diagnostic_message = result.fallback_reason;
        }
        return result;
    }

    nnue::SirioNNUE2MinimalAccumulator accumulator;
    std::string refresh_error;
    if (!nnue::refresh_sirio_nnue2_minimal_accumulator(board, *loaded_network_, accumulator,
                                                       refresh_error)) {
        result.fell_back_to_default = true;
        result.fallback_reason = "accumulator refresh rejected: " + refresh_error;
        if (diagnostic_message) {
            *diagnostic_message = result.fallback_reason;
        }
        return result;
    }
    result.accumulator_refreshed = true;

    std::int32_t white_pov_score = 0;
    std::string eval_error;
    if (!nnue::evaluate_sirio_nnue2_minimal_accumulator(accumulator, *loaded_network_, white_pov_score,
                                                        eval_error)) {
        result.fell_back_to_default = true;
        result.fallback_reason = "accumulator evaluate rejected: " + eval_error;
        if (diagnostic_message) {
            *diagnostic_message = result.fallback_reason;
        }
        return result;
    }

    result.score = board.side_to_move() == Color::White ? white_pov_score : -white_pov_score;
    result.used_experimental_route = true;
    result.fell_back_to_default = false;
    if (diagnostic_message) {
        *diagnostic_message = "experimental runtime route: SirioNNUE2-MinimalV1";
    }
    return result;
}

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


ExperimentalSirioNNUE2ShadowEvaluationResult evaluate_with_sirio_nnue2_runtime_for_tests(
    const Board &board, std::int32_t default_score, const ExperimentalSirioNNUE2Runtime &runtime,
    std::string *diagnostic_message) {
    const auto runtime_result = runtime.evaluate_with_fallback(board, default_score, diagnostic_message);

    ExperimentalSirioNNUE2ShadowEvaluationResult result{};
    result.score = runtime_result.score;
    result.used_experimental_runtime = runtime_result.used_experimental_route;
    result.fell_back_to_default = runtime_result.fell_back_to_default;
    result.runtime_active = runtime.is_active();
    result.runtime_loaded = runtime.is_loaded();
    result.runtime_status = runtime_result.status;
    result.fallback_reason = runtime_result.fallback_reason;
    return result;
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
