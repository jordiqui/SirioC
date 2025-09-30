#include "random.h"

#include <stddef.h>

void random_init(RandomState* state, uint64_t seed) {
    if (state == NULL) {
        return;
    }
    state->seed = seed ? seed : 0x9E3779B97F4A7C15ULL;
}

uint64_t random_next(RandomState* state) {
    if (state == NULL) {
        return 0ULL;
    }
    uint64_t x = state->seed;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    state->seed = x;
    return x * 0x2545F4914F6CDD1DULL;
}

