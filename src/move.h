#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

Move move_create(Square from, Square to, Piece piece, Piece capture, Piece promotion, int flags);
int move_is_null(const Move* move);

#ifdef __cplusplus
} /* extern "C" */
#endif

