#pragma once

#include "board.h"

#ifdef __cplusplus
extern "C" {
#endif

uint64_t perft_count(Board* board, int depth);

#ifdef __cplusplus
} /* extern "C" */
#endif

