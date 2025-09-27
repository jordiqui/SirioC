#include "engine/core/perft.hpp"

#include "engine/core/board.hpp"

namespace engine {

uint64_t perft(Board& board, int depth) {
    if (depth <= 0) return 1;

    auto moves = board.generate_legal_moves();
    if (depth == 1) {
        return static_cast<uint64_t>(moves.size());
    }

    uint64_t nodes = 0;
    for (Move move : moves) {
        Board::State state;
        board.apply_move(move, state);
        nodes += perft(board, depth - 1);
        board.undo_move(state);
    }
    return nodes;
}

} // namespace engine

