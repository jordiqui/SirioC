#pragma once

#include <cstdint>

#include "engine/types.hpp"

namespace engine::time {

struct Allocation {
    int64_t optimal_ms = -1;
    int64_t maximum_ms = -1;
};

Allocation compute_allocation(const Limits& limits, bool white_to_move, int move_overhead_ms);

} // namespace engine::time

