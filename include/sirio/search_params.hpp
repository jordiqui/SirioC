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
inline constexpr int continuation_history_quiet_beta_cutoff_bonus = 16;
inline constexpr int continuation_history_quiet_beta_cutoff_malus = -8;

inline constexpr int futility_margin_depth1 = 150;

} // namespace sirio::search_params
