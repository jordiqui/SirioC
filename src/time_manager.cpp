#include "sirio/time_manager.hpp"

#include <algorithm>
#include <atomic>

namespace sirio {
namespace {

std::atomic<int> time_move_overhead{10};
std::atomic<int> time_minimum_thinking{100};
std::atomic<int> time_slow_mover{100};
std::atomic<int> time_nodes_per_ms{0};

}  // namespace

void set_move_overhead(int milliseconds) {
    time_move_overhead.store(std::clamp(milliseconds, 0, 5000), std::memory_order_relaxed);
}

void set_minimum_thinking_time(int milliseconds) {
    time_minimum_thinking.store(std::clamp(milliseconds, 0, 5000), std::memory_order_relaxed);
}

void set_slow_mover(int value) {
    time_slow_mover.store(std::clamp(value, 10, 1000), std::memory_order_relaxed);
}

void set_nodestime(int value) {
    time_nodes_per_ms.store(std::clamp(value, 0, 10000), std::memory_order_relaxed);
}

int get_move_overhead() {
    return time_move_overhead.load(std::memory_order_relaxed);
}

int get_minimum_thinking_time() {
    return time_minimum_thinking.load(std::memory_order_relaxed);
}

int get_slow_mover() {
    return time_slow_mover.load(std::memory_order_relaxed);
}

int get_nodestime() {
    return time_nodes_per_ms.load(std::memory_order_relaxed);
}

}  // namespace sirio

