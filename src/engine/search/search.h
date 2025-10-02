#pragma once

#include "eval.h"
#include "movegen.h"
#include "transposition.h"

#ifdef __cplusplus
extern "C" {
#endif

void search_init(SearchContext* context, Board* board, TranspositionTable* tt, HistoryTable* history);
Move search_iterative_deepening(SearchContext* context, const SearchLimits* limits);
Value search_root(SearchContext* context, int depth, Value alpha, Value beta);

#ifdef __cplusplus
} /* extern "C" */
#endif

