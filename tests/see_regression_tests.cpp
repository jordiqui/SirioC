#include "engine/core/board.hpp"
#include "engine/eval/see.hpp"
#include "engine/types.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

engine::Move find_move(engine::Board& board, const std::string& uci) {
    auto moves = board.generate_legal_moves();
    for (engine::Move move : moves) {
        if (board.move_to_uci(move) == uci) return move;
    }
    return engine::MOVE_NONE;
}

void run_case(const std::string& fen, const std::string& uci, int expected) {
    engine::Board board;
    bool ok = board.set_fen(fen);
    assert(ok);
    engine::Move move = find_move(board, uci);
    assert(move != engine::MOVE_NONE);
    int score = engine::eval::see(board, move);
    assert(score == expected);
}

} // namespace

int main() {
    run_case("6k1/8/8/3r4/4Q3/8/8/4K3 w - - 0 1", "e4d5", 500);
    run_case("4k3/8/3p4/4p3/8/5N2/8/4K3 w - - 0 1", "f3e5", -220);
    run_case("4k3/3r4/8/3b4/8/8/8/3RK3 w - - 0 1", "d1d5", -170);
    run_case("7k/8/8/3pP3/8/8/8/4K3 w - d6 0 1", "e5d6", 100);
    run_case("4k1qr/6P1/8/8/8/8/8/4K3 w - - 0 1", "g7h8q", -400);
    return 0;
}

