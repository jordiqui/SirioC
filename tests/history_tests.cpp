#include <cassert>

#include "sirio/board.hpp"
#include "sirio/history.hpp"
#include "sirio/move.hpp"
#include "sirio/search_params.hpp"

namespace {

void test_initial_state_neutral_and_empty_killers() {
    sirio::SearchHistory history;
    sirio::Board start;
    const sirio::Move quiet = sirio::move_from_uci(start, "e2e4");

    assert(history.quiet_history_score(quiet, sirio::Color::White) == 0);
    assert(history.quiet_history_score(quiet, sirio::Color::Black) == 0);

    const auto &killers = history.killer_slots(0);
    assert(!killers[0].has_value());
    assert(!killers[1].has_value());
}

void test_is_quiet_move_predicate() {
    sirio::Board quiet_board;
    const sirio::Move quiet = sirio::move_from_uci(quiet_board, "g1f3");
    assert(sirio::is_quiet_move(quiet));

    sirio::Board capture_board{"8/8/8/3p4/4P3/8/8/8 w - - 0 1"};
    const sirio::Move capture = sirio::move_from_uci(capture_board, "e4d5");
    assert(!sirio::is_quiet_move(capture));
}

void test_quiet_history_update_bonus_clamp_and_saturation() {
    sirio::SearchHistory history;
    sirio::Board start;
    const sirio::Move quiet = sirio::move_from_uci(start, "e2e4");

    history.update_quiet_history(sirio::Color::White, quiet, 3, true);
    assert(history.quiet_history_score(quiet, sirio::Color::White) == 9);

    history.update_quiet_history(sirio::Color::White, quiet, 10'000, true);
    const int expected_after_clamped_bonus =
        9 + sirio::search_params::history_bonus_limit;
    assert(history.quiet_history_score(quiet, sirio::Color::White) == expected_after_clamped_bonus);

    for (int i = 0; i < 2000; ++i) {
        history.update_quiet_history(sirio::Color::White, quiet, 1000, true);
    }
    assert(history.quiet_history_score(quiet, sirio::Color::White) == sirio::search_params::history_max);

    for (int i = 0; i < 5000; ++i) {
        history.update_quiet_history(sirio::Color::White, quiet, 1000, false);
    }
    assert(history.quiet_history_score(quiet, sirio::Color::White) == sirio::search_params::history_min);
}

void test_store_killer_slots_and_duplicate_handling() {
    sirio::SearchHistory history;
    sirio::Board start;
    const sirio::Move first = sirio::move_from_uci(start, "e2e4");
    const sirio::Move second = sirio::move_from_uci(start, "d2d4");

    history.store_killer(first, 4);
    auto killers = history.killer_slots(4);
    assert(killers[0].has_value());
    assert(killers[0]->from == first.from && killers[0]->to == first.to);
    assert(!killers[1].has_value());

    history.store_killer(second, 4);
    killers = history.killer_slots(4);
    assert(killers[0].has_value());
    assert(killers[0]->from == second.from && killers[0]->to == second.to);
    assert(killers[1].has_value());
    assert(killers[1]->from == first.from && killers[1]->to == first.to);

    history.store_killer(second, 4);
    killers = history.killer_slots(4);
    assert(killers[0].has_value());
    assert(killers[1].has_value());
    assert(killers[0]->from == second.from && killers[0]->to == second.to);
    assert(killers[1]->from == first.from && killers[1]->to == first.to);
}

void test_isolation_between_entries() {
    sirio::SearchHistory history;
    sirio::Board start;
    const sirio::Move e2e4 = sirio::move_from_uci(start, "e2e4");
    const sirio::Move d2d4 = sirio::move_from_uci(start, "d2d4");

    history.update_quiet_history(sirio::Color::White, e2e4, 4, true);
    assert(history.quiet_history_score(e2e4, sirio::Color::White) > 0);
    assert(history.quiet_history_score(d2d4, sirio::Color::White) == 0);
    assert(history.quiet_history_score(e2e4, sirio::Color::Black) == 0);

    history.store_killer(e2e4, 2);
    const auto &ply2 = history.killer_slots(2);
    const auto &ply3 = history.killer_slots(3);
    assert(ply2[0].has_value());
    assert(!ply3[0].has_value());
    assert(!ply3[1].has_value());
}

}  // namespace

void run_history_tests() {
    test_initial_state_neutral_and_empty_killers();
    test_is_quiet_move_predicate();
    test_quiet_history_update_bonus_clamp_and_saturation();
    test_store_killer_slots_and_duplicate_handling();
    test_isolation_between_entries();
}
