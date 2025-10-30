#include "sirio/time_manager.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace sirio {
namespace {

std::atomic<int> time_move_overhead{10};
std::atomic<int> time_minimum_thinking{100};
std::atomic<int> time_slow_mover{100};
std::atomic<int> time_nodes_per_ms{0};
std::atomic<int> time_expected_moves{30};
std::atomic<int> time_latency_average{0};
std::atomic<bool> time_auto_time_tuning{true};

int clamp_moves(int moves) {
    if (moves <= 0) {
        return 30;
    }
    return std::clamp(moves, 1, 200);
}

int clamp_latency(int latency) {
    return std::clamp(latency, 0, 5000);
}

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

void set_auto_time_tuning(bool enabled) {
    time_auto_time_tuning.store(enabled, std::memory_order_relaxed);
    if (!enabled) {
        time_latency_average.store(0, std::memory_order_relaxed);
    }
}

void set_expected_moves_to_go(int moves_to_go) {
    time_expected_moves.store(clamp_moves(moves_to_go), std::memory_order_relaxed);
}

void report_time_observation(int moves_to_go, int planned_soft_limit_ms, int actual_elapsed_ms) {
    if (!time_auto_time_tuning.load(std::memory_order_relaxed)) {
        return;
    }

    set_expected_moves_to_go(moves_to_go);

    if (planned_soft_limit_ms < 0) {
        planned_soft_limit_ms = 0;
    }
    if (actual_elapsed_ms < 0) {
        actual_elapsed_ms = 0;
    }
    int latency = actual_elapsed_ms - planned_soft_limit_ms;
    if (latency < 0) {
        latency = 0;
    }
    latency = clamp_latency(latency);

    int previous = time_latency_average.load(std::memory_order_relaxed);
    int updated = previous == 0 ? latency : static_cast<int>((previous * 3 + latency) / 4);
    time_latency_average.store(updated, std::memory_order_relaxed);
}

void reset_time_manager_state() {
    time_move_overhead.store(10, std::memory_order_relaxed);
    time_minimum_thinking.store(100, std::memory_order_relaxed);
    time_slow_mover.store(100, std::memory_order_relaxed);
    time_nodes_per_ms.store(0, std::memory_order_relaxed);
    time_expected_moves.store(30, std::memory_order_relaxed);
    time_latency_average.store(0, std::memory_order_relaxed);
    time_auto_time_tuning.store(true, std::memory_order_relaxed);
}

int get_move_overhead() {
    int base = time_move_overhead.load(std::memory_order_relaxed);
    if (!time_auto_time_tuning.load(std::memory_order_relaxed)) {
        return base;
    }

    int latency = time_latency_average.load(std::memory_order_relaxed);
    if (latency <= 0) {
        return base;
    }

    int moves = time_expected_moves.load(std::memory_order_relaxed);
    moves = clamp_moves(moves);
    double move_component = static_cast<double>(moves) / 40.0;
    if (move_component < 0.0) {
        move_component = 0.0;
    }
    if (move_component > 1.5) {
        move_component = 1.5;
    }
    double scaling = 1.0 + move_component;
    int dynamic = static_cast<int>(std::llround(static_cast<double>(latency) * scaling));
    int total = base + dynamic;
    return std::clamp(total, 0, 5000);
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

bool get_auto_time_tuning() {
    return time_auto_time_tuning.load(std::memory_order_relaxed);
}

}  // namespace sirio

