#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RandomState {
    uint64_t seed;
} RandomState;

void random_init(RandomState* state, uint64_t seed);
uint64_t random_next(RandomState* state);

#ifdef __cplusplus
} /* extern "C" */
#endif

