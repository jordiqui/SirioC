#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

Move move_create(Square from, Square to, Piece piece, Piece capture, Piece promotion, int flags);
int move_is_null(const Move* move);
void move_to_uci(const Move* move, char* buffer, size_t size);

#ifdef __cplusplus
} /* extern "C" */
#endif

