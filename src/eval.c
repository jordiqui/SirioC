#include "eval.h"
#include "board.h"
#include "bits.h"
#include "nn/accumulator.h"
#include "nn/evaluate.h"

#include <stdbool.h>

static sirio_nn_model g_eval_model;
static bool g_eval_model_ready = false;

static void eval_ensure_initialized(void) {
    if (g_eval_model_ready) {
        return;
    }

    sirio_nn_model_init(&g_eval_model);
    g_eval_model_ready = true;
}

void eval_init(void) {
    eval_ensure_initialized();
}

int eval_load_network(const char* path) {
    eval_ensure_initialized();
    return sirio_nn_model_load(&g_eval_model, path);
}

Value eval_position(const Board* board) {
    if (board == NULL) {
        return VALUE_DRAW;
    }

    eval_ensure_initialized();

    sirio_accumulator accumulator;
    sirio_accumulator_reset(&accumulator);

    for (int color = COLOR_WHITE; color < COLOR_NB; ++color) {
        const sirio_nn_color nn_color =
            (color == COLOR_WHITE) ? SIRIO_NN_COLOR_WHITE : SIRIO_NN_COLOR_BLACK;

        for (int pt = PIECE_PAWN; pt < PIECE_TYPE_NB; ++pt) {
            const Bitboard bb = board->bitboards[pt + PIECE_TYPE_NB * color];
            const int count = bits_popcount(bb);
            if (count == 0) {
                continue;
            }

            const int nn_index = pt - PIECE_PAWN;
            sirio_accumulator_add(&accumulator, nn_color, nn_index, count);
        }
    }

    int score = sirio_nn_evaluate(&g_eval_model, &accumulator);
    if (board->side_to_move == COLOR_BLACK) {
        score = -score;
    }
    return score;
}

