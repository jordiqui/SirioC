#pragma once

#include "attacks.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void board_init(Board* board);
void board_set_start_position(Board* board);
int board_set_fen(Board* board, const char* fen);
Bitboard board_occupancy(const Board* board, enum Color color);
int board_is_square_attacked(const Board* board, Square square, enum Color attacker);
void board_make_move(Board* board, const Move* move);
void board_unmake_move(Board* board, const Move* move);

#ifdef __cplusplus
} /* extern "C" */
#endif

