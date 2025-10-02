#include "perft.h"
#include "movegen.h"

uint64_t perft_count(Board* board, int depth) {
    if (board == NULL || depth <= 0) {
        return 1ULL;
    }

    MoveList list;
    movegen_generate_legal_moves(board, &list);

    if (depth == 1) {
        return (uint64_t)list.size;
    }

    uint64_t nodes = 0ULL;
    for (size_t i = 0; i < list.size; ++i) {
        Move move = list.moves[i];
        board_make_move(board, &move);
        nodes += perft_count(board, depth - 1);
        board_unmake_move(board, &move);
    }

    return nodes;
}

