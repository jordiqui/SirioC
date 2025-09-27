#include "engine/util/time.hpp"

#include <algorithm>

namespace engine::time {

Allocation compute_allocation(const Limits& limits, bool white_to_move, int move_overhead_ms) {
    Allocation alloc;
    move_overhead_ms = std::max(0, move_overhead_ms);

    if (limits.movetime_ms > 0) {
        int64_t usable = std::max<int64_t>(1, limits.movetime_ms - move_overhead_ms);
        alloc.optimal_ms = usable;
        alloc.maximum_ms = std::max<int64_t>(usable, limits.movetime_ms);
        return alloc;
    }

    int64_t time_left = white_to_move ? limits.wtime_ms : limits.btime_ms;
    int64_t increment = white_to_move ? limits.winc_ms : limits.binc_ms;

    if (time_left > 0) {
        time_left = std::max<int64_t>(0, time_left - move_overhead_ms);
        int64_t base = time_left / 40;
        if (base <= 0 && time_left > 0) base = std::max<int64_t>(1, time_left / 80);
        int64_t target = base + increment;
        alloc.optimal_ms = std::max<int64_t>(1, target);
        int64_t stretch = std::max<int64_t>(alloc.optimal_ms * 3, time_left / 4);
        alloc.maximum_ms = std::max<int64_t>(alloc.optimal_ms, stretch);
    } else if (increment > 0) {
        int64_t usable_inc = std::max<int64_t>(0, increment - move_overhead_ms);
        alloc.optimal_ms = std::max<int64_t>(1, usable_inc);
        alloc.maximum_ms = std::max<int64_t>(alloc.optimal_ms, usable_inc * 2);
    }

    return alloc;
}

} // namespace engine::time

