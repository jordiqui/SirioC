#include "zobrist.h"

#include "board.h"
#include "random.h"

static uint64_t g_piece_keys[COLOR_NB][PIECE_TYPE_NB][64];
static uint64_t g_side_key;

void zobrist_init(void) {
    RandomState state;
    random_init(&state, 0xC0FFEEULL);

    for (int color = 0; color < COLOR_NB; ++color) {
        for (int piece = 0; piece < PIECE_TYPE_NB; ++piece) {
            for (int square = 0; square < 64; ++square) {
                g_piece_keys[color][piece][square] = random_next(&state);
            }
        }
    }
    g_side_key = random_next(&state);
}

uint64_t zobrist_key(const Board* board) {
    if (board == NULL) {
        return 0ULL;
    }

    uint64_t key = 0ULL;
    for (int square = 0; square < 64; ++square) {
        Piece piece = board->squares[square];
        if (piece == PIECE_NONE) {
            continue;
        }
        enum Color color = (board->bitboards[piece + PIECE_TYPE_NB * COLOR_WHITE] & (1ULL << square)) ? COLOR_WHITE : COLOR_BLACK;
        key ^= g_piece_keys[color][piece][square];
    }

    if (board->side_to_move == COLOR_BLACK) {
        key ^= g_side_key;
    }

    return key;
}

