#pragma once

#include "nn/accumulator.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int weights[SIRIO_NN_PIECE_TYPES];
    int bias;
} sirio_nn_model;

void sirio_nn_model_init(sirio_nn_model* model);
int sirio_nn_model_load(sirio_nn_model* model, const char* path);
void sirio_nn_model_set_weight(sirio_nn_model* model, int index, int value);
int sirio_nn_model_weight(const sirio_nn_model* model, int index);
void sirio_nn_model_set_bias(sirio_nn_model* model, int bias);
int sirio_nn_model_bias(const sirio_nn_model* model);
int sirio_nn_evaluate(const sirio_nn_model* model, const sirio_accumulator* accumulator);

#ifdef __cplusplus
}
#endif

