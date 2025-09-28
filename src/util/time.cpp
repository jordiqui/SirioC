#include "engine/util/time.hpp"

#include <algorithm>

namespace engine::time {

Allocation compute_allocation(const Limits& limits, bool white_to_move, int move_overhead_ms,
                              int fullmove_number, const TimeConfig& config) {
    Allocation alloc;
    move_overhead_ms = std::max(0, move_overhead_ms);

    if (limits.movetime_ms > 0) {
        int64_t usable = std::max<int64_t>(1, limits.movetime_ms - move_overhead_ms);
        alloc.optimal_ms = usable;
        alloc.maximum_ms = std::max<int64_t>(usable, limits.movetime_ms);
        alloc.base_ms = usable;
        alloc.time_left_ms = limits.movetime_ms;
        return alloc;
    }

    int64_t time_left = white_to_move ? limits.wtime_ms : limits.btime_ms;
    int64_t increment = white_to_move ? limits.winc_ms : limits.binc_ms;
    alloc.time_left_ms = time_left;
    alloc.increment_ms = increment;

    int moves_to_go = config.expected_full_moves;
    if (limits.movestogo > 0) {
        moves_to_go = limits.movestogo;
    } else {
        int expected_remaining = config.expected_full_moves - fullmove_number + 1;
        moves_to_go = std::clamp(expected_remaining, config.min_moves_to_go, config.max_moves_to_go);
    }
    moves_to_go = std::max(1, moves_to_go);
    alloc.moves_to_go = moves_to_go;

    if (time_left > 0) {
        time_left = std::max<int64_t>(0, time_left - move_overhead_ms);
    }

    int64_t usable_increment = 0;
    if (increment > 0) {
        int64_t inc_after_overhead = std::max<int64_t>(0, increment - move_overhead_ms);
        usable_increment = (inc_after_overhead * config.increment_reserve_percent) / 100;
    }
    alloc.usable_increment_ms = usable_increment;

    if (time_left > 0) {
        int64_t base = time_left / moves_to_go;
        if (base <= 0 && time_left > 0) {
            base = std::max<int64_t>(1, time_left / (moves_to_go + 1));
        }
        if (base <= 0) base = 1;
        alloc.base_ms = base;

        bool severe = base < config.severe_time_per_move_ms;
        alloc.severe_time_pressure = severe;

        int64_t healthy_bonus = 0;
        if (!severe && time_left > config.healthy_time_threshold_ms) {
            healthy_bonus = (base * config.healthy_time_bonus_percent) / 100;
        }

        int64_t optimal = base + healthy_bonus + usable_increment;
        if (severe) {
            int64_t pressure_bonus = usable_increment / 2;
            optimal = std::max<int64_t>(base + pressure_bonus, base);
        }

        double max_ratio = static_cast<double>(config.normal_max_spend_percent) / 100.0;
        if (severe) {
            max_ratio = std::max(max_ratio, static_cast<double>(config.severe_max_spend_percent) / 100.0);
        }

        bool panic = time_left <= config.panic_time_ms;
        alloc.panic_mode = panic;
        if (panic) {
            max_ratio = std::max(max_ratio, static_cast<double>(config.panic_spend_percent) / 100.0);
        }

        int64_t cap = static_cast<int64_t>(static_cast<double>(time_left) * max_ratio);
        if (cap <= 0) cap = std::max<int64_t>(1, time_left - 1);

        optimal = std::clamp<int64_t>(optimal, 1, std::max<int64_t>(1, cap));
        alloc.optimal_ms = optimal;

        int64_t stretch = std::max<int64_t>((optimal * config.max_stretch_percent) / 100,
                                            base + usable_increment * 2);
        stretch = std::max<int64_t>(stretch, optimal);
        int64_t maximum = std::min<int64_t>(stretch, std::max<int64_t>(cap, optimal));
        maximum = std::clamp<int64_t>(maximum, optimal, std::max<int64_t>(1, time_left));
        alloc.maximum_ms = maximum;
    } else if (usable_increment > 0) {
        alloc.base_ms = usable_increment;
        alloc.optimal_ms = std::max<int64_t>(1, usable_increment);
        alloc.maximum_ms = std::max<int64_t>(alloc.optimal_ms, usable_increment * 2);
    }

    return alloc;
}

} // namespace engine::time

