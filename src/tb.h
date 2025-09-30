#pragma once

#include "board.h"

#ifdef __cplusplus
extern "C" {
#endif

void tb_init(void);
Value tb_probe(const Board* board);

#ifdef __cplusplus
} /* extern "C" */
#endif

