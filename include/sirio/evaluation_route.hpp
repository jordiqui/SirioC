#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "sirio/board.hpp"
#include "sirio/nnue/backend.hpp"

namespace sirio {

enum class EvaluationRoute {
    DefaultExisting = 0,
    ExperimentalSirioNNUE2 = 1,
};

enum class ExperimentalEvaluationLoadStatus {
    NotAttempted = 0,
    Loaded = 1,
    LoadRejected = 2,
};

struct ExperimentalEvaluationConfig {
    EvaluationRoute selected_route = EvaluationRoute::DefaultExisting;
    std::optional<std::string> network_path;
};

struct ExperimentalEvaluationState {
    ExperimentalEvaluationConfig config{};
    ExperimentalEvaluationLoadStatus load_status = ExperimentalEvaluationLoadStatus::NotAttempted;
    bool load_attempted = false;
    bool load_succeeded = false;
    std::string fallback_reason;
    std::optional<nnue::Nnue2NetworkParameters> loaded_network;
};



enum class ExperimentalSirioNNUE2RuntimeStatus {
    Inactive = 0,
    Loaded = 1,
    LoadRejected = 2,
};

struct ExperimentalSirioNNUE2RuntimeResult {
    std::int32_t score = 0;
    bool used_experimental_route = false;
    bool fell_back_to_default = false;
    bool accumulator_refreshed = false;
    ExperimentalSirioNNUE2RuntimeStatus status = ExperimentalSirioNNUE2RuntimeStatus::Inactive;
    std::string fallback_reason;
};

class ExperimentalSirioNNUE2Runtime {
public:
    ExperimentalSirioNNUE2Runtime() = default;
    explicit ExperimentalSirioNNUE2Runtime(const ExperimentalEvaluationConfig &config);

    bool load_from_config(const ExperimentalEvaluationConfig &config);
    bool load_from_file(const std::string &network_path);

    [[nodiscard]] bool is_active() const;
    [[nodiscard]] bool is_loaded() const;
    [[nodiscard]] ExperimentalSirioNNUE2RuntimeStatus status() const;
    [[nodiscard]] const std::string &fallback_reason() const;
    [[nodiscard]] const nnue::Nnue2NetworkParameters *loaded_network() const;

    [[nodiscard]] ExperimentalSirioNNUE2RuntimeResult evaluate_with_fallback(
        const Board &board, std::int32_t default_score, std::string *diagnostic_message = nullptr) const;

private:
    ExperimentalSirioNNUE2RuntimeStatus status_ = ExperimentalSirioNNUE2RuntimeStatus::Inactive;
    bool active_ = false;
    std::string fallback_reason_;
    std::optional<nnue::Nnue2NetworkParameters> loaded_network_;
};


struct ExperimentalSirioNNUE2ShadowEvaluationResult {
    std::int32_t score = 0;
    bool used_experimental_runtime = false;
    bool fell_back_to_default = false;
    bool runtime_active = false;
    bool runtime_loaded = false;
    ExperimentalSirioNNUE2RuntimeStatus runtime_status = ExperimentalSirioNNUE2RuntimeStatus::Inactive;
    std::string fallback_reason;
};

[[nodiscard]] ExperimentalSirioNNUE2ShadowEvaluationResult
evaluate_with_sirio_nnue2_runtime_for_tests(const Board &board, std::int32_t default_score,
                                            const ExperimentalSirioNNUE2Runtime &runtime,
                                            std::string *diagnostic_message = nullptr);

struct EvaluationRouteResult {
    std::int32_t score = 0;
    EvaluationRoute selected_route = EvaluationRoute::DefaultExisting;
    EvaluationRoute actual_route = EvaluationRoute::DefaultExisting;
    bool used_default_route = true;
    bool used_experimental_route = false;
    bool fell_back_to_default = false;
    bool attempted_file_load = false;
    bool file_load_succeeded = false;
    std::string fallback_reason;
};

[[nodiscard]] ExperimentalEvaluationState prepare_experimental_evaluation_state_for_tests(
    const ExperimentalEvaluationConfig &config);

[[nodiscard]] EvaluationRouteResult evaluate_with_experimental_evaluation_state_for_tests(
    const Board &board, std::int32_t default_score, const ExperimentalEvaluationState &state,
    std::string *diagnostic_message = nullptr);

[[nodiscard]] EvaluationRouteResult evaluate_with_experimental_evaluation_state_for_tests(
    const Board &board, const ExperimentalEvaluationState &state,
    std::string *diagnostic_message = nullptr);

[[nodiscard]] EvaluationRouteResult evaluate_with_experimental_backend_for_tests(
    const Board &board, std::int32_t default_score, EvaluationRoute route,
    const nnue::Nnue2NetworkParameters *network, std::string *diagnostic_message = nullptr);

[[nodiscard]] EvaluationRouteResult evaluate_with_experimental_backend_for_tests(
    const Board &board, EvaluationRoute route, const nnue::Nnue2NetworkParameters *network,
    std::string *diagnostic_message = nullptr);

[[nodiscard]] EvaluationRouteResult evaluate_with_experimental_backend_file_for_tests(
    const Board &board, std::int32_t default_score, EvaluationRoute route,
    const std::string &network_path, std::string *diagnostic_message = nullptr);

[[nodiscard]] EvaluationRouteResult evaluate_with_experimental_backend_file_for_tests(
    const Board &board, EvaluationRoute route, const std::string &network_path,
    std::string *diagnostic_message = nullptr);

}  // namespace sirio
