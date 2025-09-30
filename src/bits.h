#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

int bits_count(Bitboard bb);
int bits_popcount(Bitboard bb);
Square bits_ls1b(Bitboard bb);
Bitboard bits_pop_lsb(Bitboard* bb);

#ifdef __cplusplus
} /* extern "C" */
#endif

