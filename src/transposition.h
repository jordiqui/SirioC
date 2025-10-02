#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void transposition_init(TranspositionTable* table, size_t size);
void transposition_free(TranspositionTable* table);
void transposition_store(TranspositionTable* table, uint64_t key, Value value, Move move, int depth, int flags);
const TranspositionEntry* transposition_probe(const TranspositionTable* table, uint64_t key);
int transposition_hashfull(const TranspositionTable* table);

#ifdef __cplusplus
} /* extern "C" */
#endif

