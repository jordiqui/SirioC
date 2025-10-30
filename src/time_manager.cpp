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
std::atomic<int> time_latency_estimate{0};
std::atomic<bool> time_auto_tuning{false};

constexpr int kMaxOverhead = 5000;
constexpr int kMaxLatencySample = 5000;
constexpr int kMaxMoves = 200;

int clamp_overhead(int value) { return std::clamp(value, 0, kMaxOverhead); }

int clamp_moves(int moves) {
    if (moves <= 0) {
        return 30;
    }
    return std::clamp(moves, 1, kMaxMoves);
}

int compute_dynamic_overhead(int base_overhead, int latency, int moves) {
    if (!time_auto_tuning.load(std::memory_order_relaxed)) {
        return clamp_overhead(base_overhead);
    }

    moves = clamp_moves(moves);
    if (latency < 0) {
        latency = 0;
    }

    // Increase the safety margin when the remaining move count shrinks, while keeping
    // a modest baseline for long controls. The multiplier is intentionally bounded
    // to avoid runaway growth when the latency estimate spikes.
    double moves_factor = 40.0 / static_cast<double>(moves);
    if (moves_factor < 0.5) {
        moves_factor = 0.5;
    }
    if (moves_factor > 6.0) {
        moves_factor = 6.0;
    }

    double dynamic = static_cast<double>(base_overhead) +
                     static_cast<double>(latency) * moves_factor;
    int result = static_cast<int>(std::lround(dynamic));
    return clamp_overhead(result);
}

int clamp_latency(int latency) { return std::clamp(latency, 0, kMaxLatencySample); }

}  // namespace

void set_move_overhead(int milliseconds) {
    time_move_overhead.store(clamp_overhead(milliseconds), std::memory_order_relaxed);
}

void set_minimum_thinking_time(int milliseconds) {
    time_minimum_thinking.store(clamp_overhead(milliseconds), std::memory_order_relaxed);
}

void set_slow_mover(int value) {
    time_slow_mover.store(std::clamp(value, 10, 1000), std::memory_order_relaxed);
}

void set_nodestime(int value) {
    time_nodes_per_ms.store(std::clamp(value, 0, 10000), std::memory_order_relaxed);
}

void set_auto_time_tuning(bool enabled) {
    time_auto_tuning.store(enabled, std::memory_order_relaxed);
    if (!enabled) {
        time_latency_estimate.store(0, std::memory_order_relaxed);
    }
}

void set_moves_to_go_hint(int moves) {
    time_expected_moves.store(clamp_moves(moves), std::memory_order_relaxed);
}

void set_expected_moves_to_go(int moves_to_go) { set_moves_to_go_hint(moves_to_go); }

void record_latency_sample(int milliseconds) {
    if (!time_auto_tuning.load(std::memory_order_relaxed)) {
        return;
    }
    int sample = clamp_latency(milliseconds);
    int previous = time_latency_estimate.load(std::memory_order_relaxed);
    while (true) {
        int updated = previous == 0 ? sample
                                    : static_cast<int>((static_cast<long long>(previous) * 3 + sample) / 4);
        if (updated < 0) {
            updated = 0;
        }
        if (time_latency_estimate.compare_exchange_weak(previous, updated, std::memory_order_relaxed,
                                                        std::memory_order_relaxed)) {
            break;
        }
    }
}

void report_time_observation(int moves_to_go, int planned_soft_limit_ms, int actual_elapsed_ms) {
    if (!time_auto_tuning.load(std::memory_order_relaxed)) {
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

    record_latency_sample(latency);
}

void reset_time_manager_state() {
    time_move_overhead.store(10, std::memory_order_relaxed);
    time_minimum_thinking.store(100, std::memory_order_relaxed);
    time_slow_mover.store(100, std::memory_order_relaxed);
    time_nodes_per_ms.store(0, std::memory_order_relaxed);
    time_expected_moves.store(30, std::memory_order_relaxed);
    time_latency_estimate.store(0, std::memory_order_relaxed);
    time_auto_tuning.store(false, std::memory_order_relaxed);
}

int get_move_overhead() {
    int base = time_move_overhead.load(std::memory_order_relaxed);
    int latency = time_latency_estimate.load(std::memory_order_relaxed);
    int moves = time_expected_moves.load(std::memory_order_relaxed);
    return compute_dynamic_overhead(base, latency, moves);
}

int get_minimum_thinking_time() {
    return time_minimum_thinking.load(std::memory_order_relaxed);
}

int get_slow_mover() { return time_slow_mover.load(std::memory_order_relaxed); }

int get_nodestime() { return time_nodes_per_ms.load(std::memory_order_relaxed); }

bool get_auto_time_tuning() { return time_auto_tuning.load(std::memory_order_relaxed); }

}  // namespace sirio

