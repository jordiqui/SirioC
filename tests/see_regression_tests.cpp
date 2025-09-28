#include "engine/core/board.hpp"
#include "engine/search/search.hpp"

#include <cassert>

int main() {
    using namespace engine;

    Search search;
    Limits limits;
    limits.depth = 1;

    Board board;
    bool ok = board.set_fen("6kn/6pp/8/8/8/8/2B5/5RKR w - - 0 1");
    assert(ok);
    auto greek = search.find_bestmove(board, limits);
    assert(board.move_to_uci(greek.bestmove) == "c2h7");

    ok = board.set_fen("5k1r/5p1p/8/1B6/2B3Q1/8/8/5RK1 w - - 0 1");
    assert(ok);
    auto rook = search.find_bestmove(board, limits);
    assert(board.move_to_uci(rook.bestmove) == "f1f7");

    return 0;
}
