#include "move.h"

#include <stdio.h>

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

static char promotion_to_char(Piece promotion) {
    switch (promotion) {
        case PIECE_QUEEN:
            return 'q';
        case PIECE_ROOK:
            return 'r';
        case PIECE_BISHOP:
            return 'b';
        case PIECE_KNIGHT:
            return 'n';
        default:
            return '\0';
    }
}

void move_to_uci(const Move* move, char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return;
    }

    if (!move || move_is_null(move)) {
        buffer[0] = '\0';
        return;
    }

    const char files[] = "abcdefgh";
    int from_file = move->from % 8;
    int from_rank = move->from / 8;
    int to_file = move->to % 8;
    int to_rank = move->to / 8;

    char promotion = promotion_to_char(move->promotion);

    if (promotion) {
        snprintf(buffer, size, "%c%d%c%d%c",
                 files[from_file], from_rank + 1,
                 files[to_file], to_rank + 1,
                 promotion);
    } else {
        snprintf(buffer, size, "%c%d%c%d",
                 files[from_file], from_rank + 1,
                 files[to_file], to_rank + 1);
    }
}

