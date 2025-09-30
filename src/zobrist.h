#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void zobrist_init(void);
uint64_t zobrist_key(const Board* board);

#ifdef __cplusplus
} /* extern "C" */
#endif

