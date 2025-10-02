#pragma once

#include "board.h"
#include "move.h"

#ifdef __cplusplus
extern "C" {
#endif

void movegen_generate_legal_moves(const Board* board, MoveList* list);

#ifdef __cplusplus
} /* extern "C" */
#endif

