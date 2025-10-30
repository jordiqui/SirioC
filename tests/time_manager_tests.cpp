#include <algorithm>
#include <cassert>
#include <cmath>

#include "sirio/time_manager.hpp"

namespace {

int expected_overhead(int base, int latency, int moves_to_go) {
    moves_to_go = moves_to_go <= 0 ? 30 : std::clamp(moves_to_go, 1, 200);
    double move_component = static_cast<double>(moves_to_go) / 40.0;
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

void reset_with_defaults() {
    sirio::reset_time_manager_state();
    sirio::set_auto_time_tuning(true);
}

void test_midgame_dynamic_overhead() {
    reset_with_defaults();
    sirio::set_move_overhead(20);
    sirio::set_expected_moves_to_go(30);
    sirio::report_time_observation(30, 1000, 1120);  // latency 120ms
    int expected = expected_overhead(20, 120, 30);
    assert(sirio::get_move_overhead() == expected);
}

void test_endgame_overhead_scales_down() {
    reset_with_defaults();
    sirio::set_move_overhead(15);
    sirio::set_expected_moves_to_go(5);
    sirio::report_time_observation(5, 800, 880);  // latency 80ms
    int expected = expected_overhead(15, 80, 5);
    assert(sirio::get_move_overhead() == expected);
}

void test_time_trouble_latency_decay() {
    reset_with_defaults();
    sirio::set_move_overhead(25);
    sirio::set_expected_moves_to_go(2);
    sirio::report_time_observation(2, 500, 800);  // latency 300ms
    int expected_first = expected_overhead(25, 300, 2);
    assert(sirio::get_move_overhead() == expected_first);
    sirio::report_time_observation(2, 600, 580);  // latency clamped to 0
    int expected_second = expected_overhead(25, 225, 2);
    assert(sirio::get_move_overhead() == expected_second);
}

}  // namespace

void run_time_manager_tests() {
    test_midgame_dynamic_overhead();
    test_endgame_overhead_scales_down();
    test_time_trouble_latency_decay();
}

