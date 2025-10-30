#include <cassert>

#include "sirio/time_manager.hpp"

namespace {

void apply_latency_samples(int value, int repetitions) {
    for (int i = 0; i < repetitions; ++i) {
        sirio::record_latency_sample(value);
    }
}

void test_manual_overhead_respected() {
    sirio::reset_time_manager_state();
    sirio::set_move_overhead(40);
    sirio::set_auto_time_tuning(false);
    sirio::set_moves_to_go_hint(20);
    sirio::record_latency_sample(200);
    assert(sirio::get_move_overhead() == 40);
}

void test_middlegame_distribution() {
    sirio::reset_time_manager_state();
    sirio::set_auto_time_tuning(true);
    sirio::set_move_overhead(10);
    sirio::set_moves_to_go_hint(30);
    apply_latency_samples(60, 3);
    int overhead = sirio::get_move_overhead();
    assert(overhead >= 55);
    assert(overhead <= 70);
}

void test_endgame_distribution() {
    sirio::reset_time_manager_state();
    sirio::set_auto_time_tuning(true);
    sirio::set_move_overhead(10);
    sirio::set_moves_to_go_hint(10);
    apply_latency_samples(60, 4);
    int overhead = sirio::get_move_overhead();
    assert(overhead >= 150);
}

void test_time_scramble_distribution() {
    sirio::reset_time_manager_state();
    sirio::set_auto_time_tuning(true);
    sirio::set_move_overhead(10);
    sirio::set_moves_to_go_hint(4);
    apply_latency_samples(120, 6);
    int overhead = sirio::get_move_overhead();
    assert(overhead >= 450);
}

}  // namespace

void run_time_manager_tests() {
    test_manual_overhead_respected();
    test_middlegame_distribution();
    test_endgame_distribution();
    test_time_scramble_distribution();
}

