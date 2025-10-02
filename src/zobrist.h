#pragma once

#include <stdint.h>

#include "board.h"

#ifdef __cplusplus
extern "C" {
#endif

void zobrist_init(void);
uint64_t zobrist_compute_key(const Board* board);
uint64_t zobrist_piece_key(enum Color color, Piece piece, Square square);
uint64_t zobrist_side_key(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

