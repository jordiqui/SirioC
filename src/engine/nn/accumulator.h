#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIRIO_NN_PIECE_TYPES 6
#define SIRIO_NN_COLORS 2

typedef enum {
    SIRIO_NN_COLOR_WHITE = 0,
    SIRIO_NN_COLOR_BLACK = 1
} sirio_nn_color;

typedef struct {
    int counts[SIRIO_NN_COLORS][SIRIO_NN_PIECE_TYPES];
} sirio_accumulator;

void sirio_accumulator_reset(sirio_accumulator* accumulator);
void sirio_accumulator_add(sirio_accumulator* accumulator, sirio_nn_color color, int piece_type, int delta);
int sirio_accumulator_material_delta(const sirio_accumulator* accumulator, int piece_type);

#ifdef __cplusplus
}
#endif

