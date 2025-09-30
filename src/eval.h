#pragma once

#include "types.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void eval_init(void);
void eval_shutdown(void);
int eval_load_network(const char* path);
int eval_load_network_from_buffer(const void* data, size_t size);
int eval_load_small_network(const char* path);
void eval_set_use_nnue(bool use_nnue);
bool eval_use_nnue(void);
bool eval_has_small_network(void);
Value eval_position(const Board* board);

#ifdef __cplusplus
} /* extern "C" */
#endif

