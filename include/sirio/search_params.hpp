#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

#include "sirio/board.hpp"

namespace sirio::search_params {

inline constexpr int mate_score = 100000;
inline constexpr int max_search_depth = 128;
inline constexpr int mate_threshold = mate_score - max_search_depth;

inline constexpr std::array<int, static_cast<std::size_t>(PieceType::Count)> mvv_values = {
    100, 320, 330, 500, 900, 20000};
inline constexpr std::array<int, static_cast<std::size_t>(PieceType::Count)> see_piece_values = {
    100, 320, 330, 500, 900, 20000};

inline constexpr int max_lmr_depth = 64;
inline constexpr int max_lmr_moves = 64;

inline constexpr std::uint64_t node_flush_interval = 512;
inline constexpr std::chrono::microseconds info_output_lock_timeout{500};
inline constexpr std::uint64_t time_check_interval = 2048;

inline constexpr int max_search_threads = 1024;

inline constexpr int history_bonus_limit = 8192;
inline constexpr int history_max = 16384;
inline constexpr int history_min = -history_max;
inline constexpr int continuation_history_quiet_score_scale = 1;
inline constexpr int capture_noisy_history_score_scale = 1;
inline constexpr int correction_history_max = 1024;
inline constexpr int correction_history_min = -correction_history_max;
inline constexpr int correction_history_default_bonus = 16;
inline constexpr int correction_history_runtime_delta_scale = 4;
inline constexpr int correction_history_runtime_delta_max = 64;
inline constexpr int continuation_history_quiet_beta_cutoff_bonus = 16;
inline constexpr int continuation_history_quiet_beta_cutoff_malus = -8;

inline constexpr int futility_margin_depth1 = 150;

inline constexpr bool selectivity_reverse_futility_enabled = true;
inline constexpr bool selectivity_move_count_pruning_enabled = true;
inline constexpr bool selectivity_probcut_enabled = false;
inline constexpr bool selectivity_singular_extensions_enabled = false;
inline constexpr int reverse_futility_depth_limit = 0;
inline constexpr int reverse_futility_margin_base = 0;
inline constexpr int reverse_futility_margin_per_depth = 0;
inline constexpr int reverse_futility_improving_margin_reduction = 0;
inline constexpr int move_count_pruning_depth_limit = 3;
inline constexpr int move_count_pruning_base_count = 16;
inline constexpr int move_count_pruning_depth_multiplier = 1;
inline constexpr int move_count_pruning_improving_offset = 0;
inline constexpr int probcut_depth_limit = 5;
inline constexpr int probcut_margin = 150;
inline constexpr int probcut_reduction = 2;


struct ProbCutCandidateContext {
    bool has_candidate_move = false;
    bool is_capture_or_noisy = false;
    bool is_promotion = false;
};

struct ProbCutReducedSearchResult {
    bool has_result = false;
    int value = 0;
};

struct ProbCutReducedSearchRequest {
    bool has_request = false;
    int beta = 0;
    int depth = 0;
};

[[nodiscard]] inline constexpr ProbCutCandidateContext empty_probcut_candidate_context() {
    return ProbCutCandidateContext{};
}

[[nodiscard]] inline constexpr ProbCutCandidateContext select_probcut_candidate_context() {
    return empty_probcut_candidate_context();
}

[[nodiscard]] inline constexpr ProbCutCandidateContext make_probcut_candidate_context(
    bool has_candidate_move, bool is_capture_or_noisy, bool is_promotion) {
    return ProbCutCandidateContext{has_candidate_move, is_capture_or_noisy, is_promotion};
}

[[nodiscard]] inline constexpr ProbCutCandidateContext classify_probcut_candidate(
    bool has_candidate_move, bool is_capture, bool is_noisy, bool is_promotion) {
    return ProbCutCandidateContext{
        has_candidate_move,
        has_candidate_move && (is_capture || is_noisy),
        has_candidate_move && is_promotion};
}

[[nodiscard]] inline constexpr ProbCutCandidateContext select_probcut_candidate_context_from_flags(
    bool has_candidate_move, bool is_capture, bool is_noisy, bool is_promotion) {
    return classify_probcut_candidate(has_candidate_move, is_capture, is_noisy, is_promotion);
}

[[nodiscard]] inline constexpr ProbCutReducedSearchResult empty_probcut_reduced_search_result() {
    return ProbCutReducedSearchResult{};
}

[[nodiscard]] inline constexpr ProbCutReducedSearchResult make_probcut_reduced_search_result(
    bool has_result, int value) {
    return ProbCutReducedSearchResult{has_result, value};
}

[[nodiscard]] inline constexpr ProbCutReducedSearchRequest empty_probcut_reduced_search_request() {
    return ProbCutReducedSearchRequest{};
}

[[nodiscard]] inline constexpr ProbCutReducedSearchRequest make_probcut_reduced_search_request(
    bool has_request, int beta, int depth) {
    return ProbCutReducedSearchRequest{has_request, beta, depth};
}

[[nodiscard]] inline constexpr bool selectivity_reverse_futility_is_enabled() {
    return selectivity_reverse_futility_enabled;
}

[[nodiscard]] inline constexpr bool selectivity_move_count_pruning_is_enabled() {
    return selectivity_move_count_pruning_enabled;
}

[[nodiscard]] inline constexpr bool selectivity_probcut_is_enabled() {
    return selectivity_probcut_enabled;
}

[[nodiscard]] inline constexpr bool selectivity_singular_extensions_are_enabled() {
    return selectivity_singular_extensions_enabled;
}

[[nodiscard]] inline constexpr int probcut_beta_threshold(int beta) {
    return beta + probcut_margin;
}

[[nodiscard]] inline constexpr int probcut_reduced_depth(int depth) {
    const int reduced = depth - probcut_reduction;
    return reduced < 0 ? 0 : reduced;
}

[[nodiscard]] inline constexpr bool should_cutoff_probcut(int reduced_search_value, int probcut_beta) {
    return reduced_search_value >= probcut_beta;
}

[[nodiscard]] inline constexpr int reverse_futility_margin(int depth, bool improving) {
    const int improving_reduction = improving ? reverse_futility_improving_margin_reduction : 0;
    const int raw_margin =
        reverse_futility_margin_base + (reverse_futility_margin_per_depth * depth) - improving_reduction;
    return raw_margin < 0 ? 0 : raw_margin;
}

[[nodiscard]] inline constexpr bool should_apply_reverse_futility_pruning(
    int depth, int corrected_static_eval, int beta, bool improving, bool in_check, bool is_pv_node,
    bool is_root_node) {
    if (!selectivity_reverse_futility_is_enabled()) {
        return false;
    }
    if (in_check || is_pv_node || is_root_node) {
        return false;
    }
    if (depth <= 0 || depth > reverse_futility_depth_limit) {
        return false;
    }

    const int margin = reverse_futility_margin(depth, improving);
    return corrected_static_eval - margin >= beta;
}

[[nodiscard]] inline constexpr int move_count_pruning_threshold(int depth, bool improving) {
    const int improving_offset = improving ? move_count_pruning_improving_offset : 0;
    const int raw_threshold =
        move_count_pruning_base_count + (move_count_pruning_depth_multiplier * depth) + improving_offset;
    return raw_threshold < 1 ? 1 : raw_threshold;
}

[[nodiscard]] inline constexpr bool should_apply_move_count_pruning(
    int depth, int move_count, bool improving, bool in_check, bool is_pv_node, bool is_root_node,
    bool is_quiet_move, bool is_promotion, bool is_tactical_or_noisy) {
    if (!selectivity_move_count_pruning_is_enabled()) {
        return false;
    }
    if (in_check || is_pv_node || is_root_node) {
        return false;
    }
    if (depth <= 0 || move_count <= 0) {
        return false;
    }
    if (!is_quiet_move || is_promotion || is_tactical_or_noisy) {
        return false;
    }

    if (depth > move_count_pruning_depth_limit) {
        return false;
    }
    const int threshold = move_count_pruning_threshold(depth, improving);
    return move_count > threshold;
}

[[nodiscard]] inline constexpr bool should_apply_probcut(
    int depth, int beta, int static_eval, bool in_check, bool is_pv_node, bool is_root_node,
    bool has_candidate_move, bool is_candidate_capture_or_noisy, bool is_candidate_promotion) {
    if (!selectivity_probcut_is_enabled()) {
        return false;
    }
    if (in_check || is_pv_node || is_root_node) {
        return false;
    }
    if (depth <= 0 || depth < probcut_depth_limit) {
        return false;
    }
    if (!has_candidate_move || !is_candidate_capture_or_noisy || is_candidate_promotion) {
        return false;
    }

    return static_eval >= probcut_beta_threshold(beta);
}

} // namespace sirio::search_params
