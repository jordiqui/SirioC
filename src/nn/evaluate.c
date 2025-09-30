#include "evaluate.h"
#include "nn/accumulator.h"
#include "../board.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "../bits.h"

static const int DEFAULT_WEIGHTS[SIRIO_NN_PIECE_TYPES] = {100, 320, 330, 500, 900, 0};
static const int DEFAULT_BIAS = 0;

static const char SIRIO_NNUE_MAGIC[8] = {'S', 'I', 'R', 'I', 'O', 'N', 'N', 'U'};
static const uint32_t SIRIO_NNUE_VERSION = 1U;

enum {
    SIRIO_NNUE_PERSPECTIVES = 2,
    SIRIO_NNUE_KING_SQUARES = 64,
    SIRIO_NNUE_BOARD_SQUARES = 64,
    SIRIO_NNUE_PIECE_BUCKETS = 5,
    SIRIO_NNUE_FEATURES_PER_PERSPECTIVE = SIRIO_NNUE_KING_SQUARES * SIRIO_NNUE_BOARD_SQUARES * SIRIO_NNUE_PIECE_BUCKETS,
    SIRIO_NNUE_TOTAL_FEATURES = SIRIO_NNUE_PERSPECTIVES * SIRIO_NNUE_FEATURES_PER_PERSPECTIVE,
    SIRIO_NNUE_MAX_HIDDEN = 256
};

typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t feature_count;
    uint32_t hidden_size;
    uint32_t output_scale;
} sirio_nnue_file_header;

static void apply_defaults(sirio_nn_model* model) {
    if (!model) {
        return;
    }
    for (int index = 0; index < SIRIO_NN_PIECE_TYPES; ++index) {
        model->weights[index] = DEFAULT_WEIGHTS[index];
    }
    model->bias = DEFAULT_BIAS;
    if (model->halfkp.hidden_biases) {
        free(model->halfkp.hidden_biases);
    }
    if (model->halfkp.feature_weights) {
        free(model->halfkp.feature_weights);
    }
    if (model->halfkp.output_weights) {
        free(model->halfkp.output_weights);
    }
    model->halfkp.hidden_biases = NULL;
    model->halfkp.feature_weights = NULL;
    model->halfkp.output_weights = NULL;
    model->halfkp.feature_count = 0;
    model->halfkp.hidden_size = 0;
    model->halfkp.output_scale = 1;
    model->halfkp.output_bias = 0;
    model->halfkp.loaded = 0;
}

void sirio_nn_model_init(sirio_nn_model* model) {
    if (!model) {
        return;
    }

    model->halfkp.hidden_biases = NULL;
    model->halfkp.feature_weights = NULL;
    model->halfkp.output_weights = NULL;
    apply_defaults(model);
}

void sirio_nn_model_free(sirio_nn_model* model) {
    if (!model) {
        return;
    }

    if (model->halfkp.hidden_biases) {
        free(model->halfkp.hidden_biases);
        model->halfkp.hidden_biases = NULL;
    }
    if (model->halfkp.feature_weights) {
        free(model->halfkp.feature_weights);
        model->halfkp.feature_weights = NULL;
    }
    if (model->halfkp.output_weights) {
        free(model->halfkp.output_weights);
        model->halfkp.output_weights = NULL;
    }
    model->halfkp.loaded = 0;
}

static int read_bytes(FILE* file, void* buffer, size_t length) {
    uint8_t* ptr = (uint8_t*)buffer;
    size_t remaining = length;
    while (remaining > 0) {
        size_t read_now = fread(ptr, 1, remaining, file);
        if (read_now == 0) {
            if (ferror(file) || feof(file)) {
                return 0;
            }
        }
        ptr += read_now;
        remaining -= read_now;
    }
    return 1;
}

static int validate_header(const sirio_nnue_file_header* header) {
    if (!header) {
        return 0;
    }
    if (memcmp(header->magic, SIRIO_NNUE_MAGIC, sizeof(SIRIO_NNUE_MAGIC)) != 0) {
        return 0;
    }
    if (header->version != SIRIO_NNUE_VERSION) {
        return 0;
    }
    if (header->feature_count != SIRIO_NNUE_TOTAL_FEATURES) {
        return 0;
    }
    if (header->hidden_size == 0 || header->hidden_size > SIRIO_NNUE_MAX_HIDDEN) {
        return 0;
    }
    if (header->output_scale == 0) {
        return 0;
    }
    return 1;
}

static int sirio_nn_model_load_internal(sirio_nn_model* model, FILE* file) {
    if (!model) {
        return 0;
    }

    sirio_nnue_file_header header;
    if (!read_bytes(file, &header, sizeof(header))) {
        return 0;
    }

    if (!validate_header(&header)) {
        return 0;
    }

    size_t bias_size = header.hidden_size * sizeof(int16_t);
    size_t weight_size = (size_t)header.feature_count * (size_t)header.hidden_size * sizeof(int8_t);
    size_t output_weight_size = header.hidden_size * sizeof(int16_t);

    int16_t* hidden_biases = (int16_t*)malloc(bias_size);
    int8_t* feature_weights = (int8_t*)malloc(weight_size);
    int16_t* output_weights = (int16_t*)malloc(output_weight_size);

    if (!hidden_biases || !feature_weights || !output_weights) {
        free(hidden_biases);
        free(feature_weights);
        free(output_weights);
        return 0;
    }

    int32_t output_bias = 0;

    if (!read_bytes(file, hidden_biases, bias_size) ||
        !read_bytes(file, feature_weights, weight_size) ||
        !read_bytes(file, &output_bias, sizeof(output_bias)) ||
        !read_bytes(file, output_weights, output_weight_size)) {
        free(hidden_biases);
        free(feature_weights);
        free(output_weights);
        return 0;
    }

    apply_defaults(model);

    model->halfkp.hidden_biases = hidden_biases;
    model->halfkp.feature_weights = feature_weights;
    model->halfkp.output_weights = output_weights;
    model->halfkp.feature_count = header.feature_count;
    model->halfkp.hidden_size = header.hidden_size;
    model->halfkp.output_scale = header.output_scale;
    model->halfkp.output_bias = output_bias;
    model->halfkp.loaded = 1;

    return 1;
}

int sirio_nn_model_load(sirio_nn_model* model, const char* path) {
    if (!model || !path) {
        return 0;
    }

    FILE* file = fopen(path, "rb");
    if (!file) {
        return 0;
    }

    int result = sirio_nn_model_load_internal(model, file);
    fclose(file);
    return result;
}

int sirio_nn_model_load_buffer(sirio_nn_model* model, const void* data, size_t size) {
    if (!model || !data || size < sizeof(sirio_nnue_file_header)) {
        return 0;
    }

    const uint8_t* ptr = (const uint8_t*)data;
    size_t remaining = size;

    sirio_nnue_file_header header;
    memcpy(&header, ptr, sizeof(header));
    if (!validate_header(&header)) {
        return 0;
    }

    size_t required = sizeof(header);
    size_t bias_size = header.hidden_size * sizeof(int16_t);
    size_t weight_size = (size_t)header.feature_count * (size_t)header.hidden_size * sizeof(int8_t);
    size_t output_weight_size = header.hidden_size * sizeof(int16_t);

    if (header.hidden_size == 0 || header.feature_count == 0) {
        return 0;
    }
    if (bias_size > SIZE_MAX - required) {
        return 0;
    }
    required += bias_size;
    if (weight_size > SIZE_MAX - required) {
        return 0;
    }
    required += weight_size;
    if (sizeof(int32_t) > SIZE_MAX - required) {
        return 0;
    }
    required += sizeof(int32_t);
    if (output_weight_size > SIZE_MAX - required) {
        return 0;
    }
    required += output_weight_size;
    if (remaining < required) {
        return 0;
    }

    ptr += sizeof(header);
    remaining -= sizeof(header);

    int16_t* hidden_biases = (int16_t*)malloc(bias_size);
    int8_t* feature_weights = (int8_t*)malloc(weight_size);
    int16_t* output_weights = (int16_t*)malloc(output_weight_size);
    if (!hidden_biases || !feature_weights || !output_weights) {
        free(hidden_biases);
        free(feature_weights);
        free(output_weights);
        return 0;
    }

    memcpy(hidden_biases, ptr, bias_size);
    ptr += bias_size;
    remaining -= bias_size;

    memcpy(feature_weights, ptr, weight_size);
    ptr += weight_size;
    remaining -= weight_size;

    int32_t output_bias = 0;
    memcpy(&output_bias, ptr, sizeof(output_bias));
    ptr += sizeof(output_bias);
    remaining -= sizeof(output_bias);

    memcpy(output_weights, ptr, output_weight_size);
    (void)remaining;

    apply_defaults(model);

    model->halfkp.hidden_biases = hidden_biases;
    model->halfkp.feature_weights = feature_weights;
    model->halfkp.output_weights = output_weights;
    model->halfkp.feature_count = header.feature_count;
    model->halfkp.hidden_size = header.hidden_size;
    model->halfkp.output_scale = header.output_scale;
    model->halfkp.output_bias = output_bias;
    model->halfkp.loaded = 1;

    return 1;
}

int sirio_nn_model_ready(const sirio_nn_model* model) {
    if (!model) {
        return 0;
    }
    return model->halfkp.loaded;
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

static inline Square orient_square(enum Color perspective, Square sq) {
    return (perspective == COLOR_WHITE) ? sq : (sq ^ 56);
}

static inline int feature_index(enum Color perspective, Square king_sq, Square sq, Piece piece) {
    const int piece_index = piece - PIECE_PAWN;
    if (piece_index < 0 || piece_index >= SIRIO_NNUE_PIECE_BUCKETS) {
        return -1;
    }

    const int oriented_king = orient_square(perspective, king_sq);
    const int oriented_sq = orient_square(perspective, sq);
    const int base = (perspective == COLOR_WHITE) ? 0 : SIRIO_NNUE_FEATURES_PER_PERSPECTIVE;
    const int index = base + ((oriented_king * SIRIO_NNUE_BOARD_SQUARES + oriented_sq) * SIRIO_NNUE_PIECE_BUCKETS + piece_index);
    if (index < 0 || (uint32_t)index >= SIRIO_NNUE_TOTAL_FEATURES) {
        return -1;
    }
    return index;
}

static Square king_square(const Board* board, enum Color color) {
    const Bitboard king_bb = board->bitboards[PIECE_KING + PIECE_TYPE_NB * color];
    if (!king_bb) {
        return -1;
    }
    return bits_ls1b(king_bb);
}

static int simple_material(const sirio_nn_model* model, const Board* board) {
    long total = model->bias;
    for (int color = COLOR_WHITE; color < COLOR_NB; ++color) {
        const int sign = (color == COLOR_WHITE) ? 1 : -1;
        for (Piece piece = PIECE_PAWN; piece < PIECE_KING; ++piece) {
            const Bitboard bb = board->bitboards[piece + PIECE_TYPE_NB * color];
            const int count = bits_popcount(bb);
            if (!count) {
                continue;
            }
            const int index = piece - PIECE_PAWN;
            total += sign * (long)model->weights[index] * (long)count;
        }
    }
    if (total > VALUE_MATE) {
        total = VALUE_MATE;
    } else if (total < -VALUE_MATE) {
        total = -VALUE_MATE;
    }
    return (int)total;
}

int sirio_nn_evaluate_board(const sirio_nn_model* model, const struct Board* board) {
    if (!model || !board) {
        return 0;
    }

    if (!model->halfkp.loaded) {
        return simple_material(model, board);
    }

    Square white_king = king_square(board, COLOR_WHITE);
    Square black_king = king_square(board, COLOR_BLACK);
    if (white_king < 0 || black_king < 0) {
        return simple_material(model, board);
    }

    int32_t hidden[SIRIO_NNUE_MAX_HIDDEN];
    for (uint32_t i = 0; i < model->halfkp.hidden_size; ++i) {
        hidden[i] = model->halfkp.hidden_biases[i];
    }

    const uint32_t hidden_size = model->halfkp.hidden_size;
    const int8_t* feature_weights = model->halfkp.feature_weights;

    for (enum Color perspective = COLOR_WHITE; perspective <= COLOR_BLACK; ++perspective) {
        const Square king_sq = (perspective == COLOR_WHITE) ? white_king : black_king;
        for (Piece piece = PIECE_PAWN; piece < PIECE_KING; ++piece) {
            Bitboard bb = board->bitboards[piece + PIECE_TYPE_NB * perspective];
            while (bb) {
                const Square sq = bits_ls1b(bb);
                bb &= bb - 1ULL;
                const int idx = feature_index(perspective, king_sq, sq, piece);
                if (idx < 0) {
                    continue;
                }
                const int8_t* weights = feature_weights + (size_t)idx * hidden_size;
                for (uint32_t h = 0; h < hidden_size; ++h) {
                    hidden[h] += weights[h];
                }
            }
        }
    }

    int64_t acc = model->halfkp.output_bias;
    for (uint32_t h = 0; h < hidden_size; ++h) {
        acc += (int64_t)hidden[h] * (int64_t)model->halfkp.output_weights[h];
    }

    if (model->halfkp.output_scale > 1) {
        acc /= (int64_t)model->halfkp.output_scale;
    }

    if (acc > VALUE_MATE) {
        acc = VALUE_MATE;
    } else if (acc < -VALUE_MATE) {
        acc = -VALUE_MATE;
    }

    return (int)acc;
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

    if (total > VALUE_MATE) {
        total = VALUE_MATE;
    } else if (total < -VALUE_MATE) {
        total = -VALUE_MATE;
    }

    return (int)total;
}

