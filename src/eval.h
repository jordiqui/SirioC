#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void eval_init(void);
int eval_load_network(const char* path);
Value eval_position(const Board* board);

#ifdef __cplusplus
} /* extern "C" */
#endif

