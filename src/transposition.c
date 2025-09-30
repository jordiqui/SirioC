#include "transposition.h"

#include <stdlib.h>

#include "util.h"

void transposition_init(TranspositionTable* table, size_t size) {
    if (table == NULL || size == 0) {
        return;
    }
    table->size = size;
    table->entries = (TranspositionEntry*)calloc(size, sizeof(TranspositionEntry));
}

void transposition_free(TranspositionTable* table) {
    if (table == NULL) {
        return;
    }
    free(table->entries);
    table->entries = NULL;
    table->size = 0;
}

void transposition_store(TranspositionTable* table, uint64_t key, Value value, Move move, int depth, int flags) {
    if (table == NULL || table->entries == NULL || table->size == 0) {
        return;
    }
    size_t index = key % table->size;
    TranspositionEntry* entry = &table->entries[index];
    entry->key = key;
    entry->value = value;
    entry->best_move = move;
    entry->depth = depth;
    entry->flags = flags;
}

const TranspositionEntry* transposition_probe(const TranspositionTable* table, uint64_t key) {
    if (table == NULL || table->entries == NULL || table->size == 0) {
        return NULL;
    }
    size_t index = key % table->size;
    const TranspositionEntry* entry = &table->entries[index];
    if (entry->key == key) {
        return entry;
    }
    return NULL;
}

