#include "eval.h"
#include "board.h"
#include "bits.h"

Value eval_position(const Board* board) {
    if (board == NULL) {
        return VALUE_DRAW;
    }

    Value material = 0;
    static const Value piece_values[PIECE_TYPE_NB] = {
        0, 100, 320, 330, 500, 900, 20000
    };

    for (int color = COLOR_WHITE; color < COLOR_NB; ++color) {
        Value color_sum = 0;
        for (int pt = PIECE_PAWN; pt < PIECE_TYPE_NB; ++pt) {
            Bitboard bb = board->bitboards[pt + PIECE_TYPE_NB * color];
            color_sum += piece_values[pt] * bits_popcount(bb);
        }
        if (color == COLOR_WHITE) {
            material += color_sum;
        } else {
            material -= color_sum;
        }
    }

    if (board->side_to_move == COLOR_WHITE) {
        return material;
    }
    return -material;
}

