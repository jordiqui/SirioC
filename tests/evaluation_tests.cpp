#include <cassert>
#include <optional>
#include <string>
#include <vector>

#include "sirio/board.hpp"
#include "sirio/evaluation.hpp"

namespace {

void test_passed_pawn_scaling() {
    sirio::Board midgame{"r3k2r/ppp2ppp/8/8/3P4/8/PPP2PPP/R3K2R w KQkq - 0 1"};
    sirio::initialize_evaluation(midgame);
    int mid_eval = sirio::evaluate(midgame);

    sirio::Board endgame{"6k1/8/4P3/8/3K4/8/8/8 w - - 0 1"};
    sirio::initialize_evaluation(endgame);
    int end_eval = sirio::evaluate(endgame);

    assert(end_eval > mid_eval);
    assert(end_eval > 0);
}

void test_king_safety_tapering() {
    sirio::Board exposed_mid{"r4rk1/ppp2ppp/8/8/8/6q1/PP3PPP/R4RK1 w - - 0 1"};
    sirio::initialize_evaluation(exposed_mid);
    int mid_eval = sirio::evaluate(exposed_mid);

    sirio::Board exposed_end{"6k1/8/8/8/8/6q1/7P/5RK1 w - - 0 1"};
    sirio::initialize_evaluation(exposed_end);
    int end_eval = sirio::evaluate(exposed_end);

    assert(mid_eval < end_eval);
}

void test_pawn_cache_stability_on_non_pawn_moves() {
    sirio::Board board{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
    sirio::initialize_evaluation(board);
    int initial_eval = sirio::evaluate(board);
    std::size_t baseline_misses = sirio::classical_evaluation_pawn_cache_misses();

    std::vector<std::string> moves{"g1f3", "g8f6", "b1c3", "b8c6"};
    std::vector<sirio::Board> history;
    history.push_back(board);

    sirio::Board current = board;
    for (const auto &uci : moves) {
        sirio::Move move = sirio::move_from_uci(current, uci);
        sirio::Board next = current.apply_move(move);
        sirio::push_evaluation_state(current.side_to_move(), std::optional<sirio::Move>{move}, next);
        (void)sirio::evaluate(next);
        assert(sirio::classical_evaluation_pawn_cache_misses() == baseline_misses);
        history.push_back(next);
        current = next;
    }

    for (std::size_t i = moves.size(); i-- > 0;) {
        sirio::pop_evaluation_state();
        current = history[i];
        (void)sirio::evaluate(current);
        assert(sirio::classical_evaluation_pawn_cache_misses() == baseline_misses);
    }

    int final_eval = sirio::evaluate(board);
    assert(final_eval == initial_eval);
    assert(sirio::classical_evaluation_pawn_cache_misses() == baseline_misses);
}

}  // namespace

void run_evaluation_phase_tests() {
    sirio::use_classical_evaluation();
    test_passed_pawn_scaling();
    test_king_safety_tapering();
    test_pawn_cache_stability_on_non_pawn_moves();
}
