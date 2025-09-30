#include "nn/evaluate.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nn/accumulator.h"

static const int DEFAULT_WEIGHTS[SIRIO_NN_PIECE_TYPES] = {100, 325, 330, 500, 900, 20000};
static const int DEFAULT_BIAS = 0;

static char* trim(char* text) {
    if (!text) {
        return text;
    }

    while (*text && isspace((unsigned char)*text)) {
        ++text;
    }

    char* end = text + strlen(text);
    while (end > text && isspace((unsigned char)*(end - 1))) {
        --end;
    }

    *end = '\0';
    return text;
}

static void apply_defaults(sirio_nn_model* model) {
    for (int index = 0; index < SIRIO_NN_PIECE_TYPES; ++index) {
        model->weights[index] = DEFAULT_WEIGHTS[index];
    }
    model->bias = DEFAULT_BIAS;
}

void sirio_nn_model_init(sirio_nn_model* model) {
    if (!model) {
        return;
    }

    apply_defaults(model);
}

static int parse_weight_name(const char* name) {
    if (!name) {
        return -1;
    }

    if (strcmp(name, "pawn") == 0 || strcmp(name, "PAWN") == 0) {
        return 0;
    }
    if (strcmp(name, "knight") == 0 || strcmp(name, "KNIGHT") == 0) {
        return 1;
    }
    if (strcmp(name, "bishop") == 0 || strcmp(name, "BISHOP") == 0) {
        return 2;
    }
    if (strcmp(name, "rook") == 0 || strcmp(name, "ROOK") == 0) {
        return 3;
    }
    if (strcmp(name, "queen") == 0 || strcmp(name, "QUEEN") == 0) {
        return 4;
    }
    if (strcmp(name, "king") == 0 || strcmp(name, "KING") == 0) {
        return 5;
    }
    return -1;
}

int sirio_nn_model_load(sirio_nn_model* model, const char* path) {
    if (!model) {
        return 0;
    }

    apply_defaults(model);

    if (!path) {
        return 0;
    }

    FILE* file = fopen(path, "r");
    if (!file) {
        return 0;
    }

    char buffer[128];
    int parsed = 0;

    while (fgets(buffer, sizeof(buffer), file)) {
        char* line = trim(buffer);
        if (*line == '\0' || *line == '#') {
            continue;
        }

        char* equals = strchr(line, '=');
        if (!equals) {
            continue;
        }

        *equals = '\0';
        const char* name = trim(line);
        const char* value = trim(equals + 1);

        if (strcmp(name, "bias") == 0 || strcmp(name, "BIAS") == 0) {
            model->bias = atoi(value);
            ++parsed;
            continue;
        }

        int index = parse_weight_name(name);
        if (index < 0) {
            continue;
        }

        model->weights[index] = atoi(value);
        ++parsed;
    }

    fclose(file);
    return parsed > 0;
}

void sirio_nn_model_set_weight(sirio_nn_model* model, int index, int value) {
    if (!model) {
        return;
    }

    if (index < 0 || index >= SIRIO_NN_PIECE_TYPES) {
        return;
    }

    model->weights[index] = value;
}

int sirio_nn_model_weight(const sirio_nn_model* model, int index) {
    if (!model) {
        return 0;
    }

    if (index < 0 || index >= SIRIO_NN_PIECE_TYPES) {
        return 0;
    }

    return model->weights[index];
}

void sirio_nn_model_set_bias(sirio_nn_model* model, int bias) {
    if (!model) {
        return;
    }

    model->bias = bias;
}

int sirio_nn_model_bias(const sirio_nn_model* model) {
    if (!model) {
        return 0;
    }

    return model->bias;
}

int sirio_nn_evaluate(const sirio_nn_model* model, const sirio_accumulator* accumulator) {
    if (!model || !accumulator) {
        return 0;
    }

    long total = model->bias;
    for (int index = 0; index < SIRIO_NN_PIECE_TYPES; ++index) {
        const int delta = sirio_accumulator_material_delta(accumulator, index);
        total += (long)delta * (long)model->weights[index];
    }

    if (total > 32767) {
        total = 32767;
    } else if (total < -32768) {
        total = -32768;
    }

    return (int)total;
}

