#include <algorithm>
#include <cassert>
#include <cmath>

#include "sirio/time_manager.hpp"

namespace {

int expected_overhead(int base, int latency, int moves_to_go) {
    moves_to_go = moves_to_go <= 0 ? 30 : std::clamp(moves_to_go, 1, 200);
    double moves_factor = 40.0 / static_cast<double>(moves_to_go);
    if (moves_factor < 0.5) {
        moves_factor = 0.5;
    }
    if (moves_factor > 6.0) {
        moves_factor = 6.0;
    }
    double dynamic = static_cast<double>(base) + static_cast<double>(latency) * moves_factor;
    int total = static_cast<int>(std::lround(dynamic));
    return std::clamp(total, 0, 5000);
}

void reset_defaults() {
    sirio::reset_time_manager_state();
    sirio::set_auto_time_tuning(true);
}

void test_manual_overhead_respected() {
    sirio::reset_time_manager_state();
    sirio::set_move_overhead(40);
    sirio::set_auto_time_tuning(false);
    sirio::set_moves_to_go_hint(20);
    sirio::record_latency_sample(200);
    assert(sirio::get_move_overhead() == 40);
}

void test_latency_samples_adjust_overhead() {
    reset_defaults();
    sirio::set_move_overhead(10);
    sirio::set_moves_to_go_hint(30);
    sirio::record_latency_sample(60);
    sirio::record_latency_sample(60);
    int expected = expected_overhead(10, 60, 30);
    assert(sirio::get_move_overhead() == expected);
}

void test_low_moves_increase_margin() {
    reset_defaults();
    sirio::set_move_overhead(10);
    sirio::set_moves_to_go_hint(5);
    sirio::record_latency_sample(80);
    int expected = expected_overhead(10, 80, 5);
    assert(sirio::get_move_overhead() == expected);
}

void test_report_time_observation_updates_state() {
    reset_defaults();
    sirio::set_move_overhead(20);
    sirio::report_time_observation(4, 500, 640);  // latency 140ms
    int expected = expected_overhead(20, 140, 4);
    assert(sirio::get_move_overhead() == expected);
}

void test_report_time_observation_clamps_negative() {
    reset_defaults();
    sirio::set_move_overhead(25);
    sirio::report_time_observation(2, 600, 550);  // negative latency clamped to 0
    int expected = expected_overhead(25, 0, 2);
    assert(sirio::get_move_overhead() == expected);
}

}  // namespace

void run_time_manager_tests() {
    test_manual_overhead_respected();
    test_latency_samples_adjust_overhead();
    test_low_moves_increase_margin();
    test_report_time_observation_updates_state();
    test_report_time_observation_clamps_negative();
}

