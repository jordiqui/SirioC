#pragma once

#include <stddef.h>
#include <stdint.h>

#include "accumulator.h"

struct Board;

#ifdef __cplusplus
extern "C" {
#endif

#define SIRIO_NN_PIECE_TYPES 6

typedef struct {
    int loaded;
    uint32_t feature_count;
    uint32_t hidden_size;
    uint32_t output_scale;
    int16_t* hidden_biases;
    int8_t* feature_weights;
    int32_t output_bias;
    int16_t* output_weights;
} sirio_nn_halfkp;

typedef struct {
    int weights[SIRIO_NN_PIECE_TYPES];
    int bias;
    sirio_nn_halfkp halfkp;
} sirio_nn_model;

void sirio_nn_model_init(sirio_nn_model* model);
void sirio_nn_model_free(sirio_nn_model* model);
int sirio_nn_model_load(sirio_nn_model* model, const char* path);
int sirio_nn_model_load_buffer(sirio_nn_model* model, const void* data, size_t size);
int sirio_nn_model_ready(const sirio_nn_model* model);
void sirio_nn_model_set_weight(sirio_nn_model* model, int index, int value);
int sirio_nn_model_weight(const sirio_nn_model* model, int index);
void sirio_nn_model_set_bias(sirio_nn_model* model, int bias);
int sirio_nn_model_bias(const sirio_nn_model* model);
int sirio_nn_evaluate_board(const sirio_nn_model* model, const struct Board* board);
int sirio_nn_evaluate(const sirio_nn_model* model, const sirio_accumulator* accumulator);

#ifdef __cplusplus
}
#endif

