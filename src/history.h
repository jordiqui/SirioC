#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void history_init(HistoryTable* history);
void history_update(HistoryTable* history, const Move* move, int bonus);
int history_get(const HistoryTable* history, const Move* move);

#ifdef __cplusplus
} /* extern "C" */
#endif

