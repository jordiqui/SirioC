#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void attacks_init(void);
Bitboard attacks_knight(Square square);
Bitboard attacks_king(Square square);
Bitboard attacks_pawn(Square square, enum Color color);
Bitboard attacks_bishop(Square square, Bitboard occupancy);
Bitboard attacks_rook(Square square, Bitboard occupancy);
Bitboard attacks_queen(Square square, Bitboard occupancy);

#ifdef __cplusplus
} /* extern "C" */
#endif

