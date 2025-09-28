#include "engine/core/board.hpp"
#include "engine/eval/eval.hpp"

#include <cassert>
#include <iostream>

namespace {

int eval_from_white(const engine::Board& board) {
    int score = engine::eval::evaluate(board);
    return board.white_to_move() ? score : -score;
}

} // namespace

int main() {
    using engine::Board;

    Board safe_king;
    assert(safe_king.set_fen("6rk/8/8/8/8/8/6PP/6K1 w - - 0 1"));
    int shield_eval = eval_from_white(safe_king);

    Board exposed_king;
    assert(exposed_king.set_fen("6rk/8/8/8/8/8/5P1P/6K1 w - - 0 1"));
    int exposed_eval = eval_from_white(exposed_king);
    assert(exposed_eval < shield_eval);

    Board start;
    int start_eval = eval_from_white(start);

    Board pawn_push = start;
    assert(pawn_push.make_move_uci("g2g4"));
    int after_push_eval = eval_from_white(pawn_push);
    assert(after_push_eval < start_eval);

    Board blocked_passed;
    assert(blocked_passed.set_fen("4k3/8/4p3/4P3/8/8/8/4K3 w - - 0 1"));
    Board clear_passed;
    assert(clear_passed.set_fen("4k3/8/8/4P3/8/8/8/4K3 w - - 0 1"));
    assert(eval_from_white(clear_passed) > eval_from_white(blocked_passed));

    Board doubled_pawns;
    assert(doubled_pawns.set_fen("4k3/8/8/8/8/4P3/4P3/4K3 w - - 0 1"));
    Board healthy_pawns;
    assert(healthy_pawns.set_fen("4k3/8/8/8/8/8/4PP2/4K3 w - - 0 1"));
    assert(eval_from_white(healthy_pawns) > eval_from_white(doubled_pawns));

    std::cout << "Evaluation heuristic tests passed.\n";
    return 0;
}
