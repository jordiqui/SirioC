#pragma once

#include "board.h"

#ifdef __cplusplus
extern "C" {
#endif

void tb_initialize(void);
Value tb_probe(const Board* board);
int tb_probe_root_position(const Board* board, Move* out_move, Value* out_value);
void tb_set_path(const char* path);
const char* tb_get_path(void);
void tb_set_probe_depth(int depth);
int tb_get_probe_depth(void);
void tb_set_50_move_rule(int enabled);
int tb_get_50_move_rule(void);
void tb_set_probe_limit(int limit);
int tb_get_probe_limit(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

