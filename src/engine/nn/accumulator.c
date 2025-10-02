#include "accumulator.h"

#include <stddef.h>
#include <string.h>

void sirio_accumulator_reset(sirio_accumulator* accumulator) {
    if (!accumulator) {
        return;
    }

    memset(accumulator->counts, 0, sizeof(accumulator->counts));
}

void sirio_accumulator_add(sirio_accumulator* accumulator, sirio_nn_color color, int piece_type, int delta) {
    if (!accumulator) {
        return;
    }

    if (piece_type < 0 || piece_type >= SIRIO_NN_PIECE_TYPES) {
        return;
    }

    if (color != SIRIO_NN_COLOR_WHITE && color != SIRIO_NN_COLOR_BLACK) {
        return;
    }

    accumulator->counts[color][piece_type] += delta;
}

int sirio_accumulator_material_delta(const sirio_accumulator* accumulator, int piece_type) {
    if (!accumulator) {
        return 0;
    }

    if (piece_type < 0 || piece_type >= SIRIO_NN_PIECE_TYPES) {
        return 0;
    }

    return accumulator->counts[SIRIO_NN_COLOR_WHITE][piece_type] -
           accumulator->counts[SIRIO_NN_COLOR_BLACK][piece_type];
}

