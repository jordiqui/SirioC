#include <cassert>

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

}  // namespace

void run_evaluation_phase_tests() {
    sirio::use_classical_evaluation();
    test_passed_pawn_scaling();
    test_king_safety_tapering();
}
