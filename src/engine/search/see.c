#include "see.h"
#include "attacks.h"

Value see_evaluate(const Board* board, Square square) {
    if (board == NULL || square < 0 || square >= 64) {
        return VALUE_DRAW;
    }

    enum Color attacker = board->side_to_move;
    return board_is_square_attacked(board, square, attacker) ? 100 : 0;
}

