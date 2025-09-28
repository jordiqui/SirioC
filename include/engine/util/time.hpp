#pragma once

#include <cstdint>

#include "engine/types.hpp"

namespace engine::time {

struct Allocation {
    int64_t optimal_ms = -1;
    int64_t maximum_ms = -1;
    int64_t base_ms = -1;
    int64_t time_left_ms = -1;
    int64_t increment_ms = -1;
    int64_t usable_increment_ms = -1;
    int moves_to_go = -1;
    bool severe_time_pressure = false;
    bool panic_mode = false;
};

struct TimeConfig {
    int expected_full_moves = 80;
    int min_moves_to_go = 6;
    int max_moves_to_go = 60;
    int healthy_time_threshold_ms = 180'000;
    int healthy_time_bonus_percent = 20;
    int panic_time_ms = 60'000;
    int panic_spend_percent = 65;
    int severe_time_per_move_ms = 1'500;
    int normal_max_spend_percent = 45;
    int severe_max_spend_percent = 55;
    int max_stretch_percent = 250;
    int increment_reserve_percent = 70;
};

Allocation compute_allocation(const Limits& limits, bool white_to_move, int move_overhead_ms,
                              int fullmove_number, const TimeConfig& config);

} // namespace engine::time

