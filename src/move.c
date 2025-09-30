#include "move.h"

Move move_create(Square from, Square to, Piece piece, Piece capture, Piece promotion, int flags) {
    Move move;
    move.from = from;
    move.to = to;
    move.piece = piece;
    move.capture = capture;
    move.promotion = promotion;
    move.flags = flags;
    return move;
}

int move_is_null(const Move* move) {
    if (move == NULL) {
        return 1;
    }
    return move->from == move->to && move->piece == PIECE_NONE;
}

