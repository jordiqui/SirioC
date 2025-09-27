#include "engine/core/board.hpp"
#include "engine/search/search.hpp"
#include <cassert>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
std::unordered_set<std::string> moves_to_set(engine::Board& board,
                                             const std::vector<engine::Move>& moves) {
    std::unordered_set<std::string> out;
    out.reserve(moves.size());
    for (engine::Move move : moves) {
        out.insert(board.move_to_uci(move));
    }
    return out;
}
} // namespace

int main() {
    using namespace engine;

    Board board;
    board.set_startpos();
    auto start_moves = board.generate_legal_moves();
    auto start_set = moves_to_set(board, start_moves);
    assert(start_moves.size() == 20);
    assert(start_set.contains("e2e4"));
    assert(start_set.contains("g1f3"));
    assert(!board.make_move_uci("e2e5"));

    board.set_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    auto castle_moves = board.generate_legal_moves();
    auto castle_set = moves_to_set(board, castle_moves);
    assert(castle_set.contains("e1g1"));
    assert(castle_set.contains("e1c1"));

    board.set_fen("r3k2r/8/8/8/8/8/5r2/R3K2R w KQ - 0 1");
    auto blocked_moves = board.generate_legal_moves();
    auto blocked_set = moves_to_set(board, blocked_moves);
    assert(!blocked_set.contains("e1g1"));

    board.set_fen("7k/8/8/3pP3/8/8/8/7K w - d6 0 1");
    auto ep_moves = board.generate_legal_moves();
    auto ep_set = moves_to_set(board, ep_moves);
    assert(ep_set.contains("e5d6"));
    assert(board.make_move_uci("e5d6"));
    assert(!board.white_to_move());

    board.set_fen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    auto no_moves = board.generate_legal_moves();
    assert(no_moves.empty());
    Search search;
    auto best = search.find_bestmove(board, Limits{});
    assert(best.bestmove == MOVE_NONE);

    return 0;
}
