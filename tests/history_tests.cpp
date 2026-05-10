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

void test_capture_history_scaffold_basics() {
    sirio::SearchHistory history;
    sirio::Board capture_board{"8/8/8/3p4/4P3/8/8/8 w - - 0 1"};
    const sirio::Move e4d5 = sirio::move_from_uci(capture_board, "e4d5");

    assert(history.capture_history().score(e4d5, sirio::Color::White) == 0);
    history.capture_history().update(sirio::Color::White, e4d5, 3, true);
    assert(history.capture_history().score(e4d5, sirio::Color::White) == 9);
    history.capture_history().update(sirio::Color::White, e4d5, 3, false);
    assert(history.capture_history().score(e4d5, sirio::Color::White) == 0);
}

void test_capture_history_clamp_and_indexing() {
    sirio::SearchHistory history;
    sirio::Board board_a{"8/8/8/3p4/4P3/8/8/8 w - - 0 1"};
    sirio::Board board_b{"8/8/8/3n4/4P3/8/8/8 w - - 0 1"};
    const sirio::Move pawn_takes_pawn = sirio::move_from_uci(board_a, "e4d5");
    const sirio::Move pawn_takes_knight = sirio::move_from_uci(board_b, "e4d5");

    history.capture_history().update(sirio::Color::White, pawn_takes_pawn, 1000, true);
    assert(history.capture_history().score(pawn_takes_pawn, sirio::Color::White) ==
           sirio::search_params::history_bonus_limit);
    assert(history.capture_history().score(pawn_takes_knight, sirio::Color::White) == 0);
    assert(history.capture_history().score(pawn_takes_pawn, sirio::Color::Black) == 0);

    for (int i = 0; i < 5000; ++i) {
        history.capture_history().update(sirio::Color::White, pawn_takes_pawn, 1000, true);
    }
    assert(history.capture_history().score(pawn_takes_pawn, sirio::Color::White) ==
           sirio::search_params::history_max);
}

void test_noisy_history_scaffold_basics_and_determinism() {
    sirio::SearchHistory history;
    sirio::Board promotion_board{"4k3/6P1/8/8/8/8/8/4K3 w - - 0 1"};
    const sirio::Move g7g8q = sirio::move_from_uci(promotion_board, "g7g8q");
    const sirio::Move g7g8r = sirio::move_from_uci(promotion_board, "g7g8r");

    assert(history.noisy_history().score(g7g8q, sirio::Color::White) == 0);
    history.noisy_history().update(sirio::Color::White, g7g8q, 2, true);
    assert(history.noisy_history().score(g7g8q, sirio::Color::White) == 4);
    assert(history.noisy_history().score(g7g8r, sirio::Color::White) == 4);

    history.noisy_history().update(sirio::Color::White, g7g8q, 2, false);
    assert(history.noisy_history().score(g7g8q, sirio::Color::White) == 0);
    assert(history.noisy_history().score(g7g8r, sirio::Color::White) == 0);
}

void test_history_clear_resets_quiet_killer_capture_and_noisy() {
    sirio::SearchHistory history;
    sirio::Board start;
    sirio::Board capture_board{"8/8/8/3p4/4P3/8/8/8 w - - 0 1"};
    sirio::Board promotion_board{"4k3/6P1/8/8/8/8/8/4K3 w - - 0 1"};
    const sirio::Move quiet = sirio::move_from_uci(start, "e2e4");
    const sirio::Move capture = sirio::move_from_uci(capture_board, "e4d5");
    const sirio::Move noisy = sirio::move_from_uci(promotion_board, "g7g8q");

    history.update_quiet_history(sirio::Color::White, quiet, 3, true);
    history.store_killer(quiet, 1);
    history.capture_history().update(sirio::Color::White, capture, 3, true);
    history.noisy_history().update(sirio::Color::White, noisy, 3, true);
    history.clear();

    assert(history.quiet_history_score(quiet, sirio::Color::White) == 0);
    assert(!history.killer_slots(1)[0].has_value());
    assert(history.capture_history().score(capture, sirio::Color::White) == 0);
    assert(history.noisy_history().score(noisy, sirio::Color::White) == 0);
}

}  // namespace

void run_history_tests() {
    test_initial_state_neutral_and_empty_killers();
    test_is_quiet_move_predicate();
    test_quiet_history_update_bonus_clamp_and_saturation();
    test_store_killer_slots_and_duplicate_handling();
    test_isolation_between_entries();
    test_capture_history_scaffold_basics();
    test_capture_history_clamp_and_indexing();
    test_noisy_history_scaffold_basics_and_determinism();
    test_history_clear_resets_quiet_killer_capture_and_noisy();
}
