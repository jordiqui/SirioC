#include "eval.h"
#include "board.h"
#include "bits.h"
#include "nn/evaluate.h"

#include <stdbool.h>
#include <stdio.h>

#define SIRIO_DEFAULT_NETWORK "resources/sirio_default.nnue"
#define SIRIO_DEFAULT_NETWORK_ALT "../resources/sirio_default.nnue"
#define SIRIO_DEFAULT_SMALL_NETWORK "resources/sirio_small.nnue"
#define SIRIO_DEFAULT_SMALL_NETWORK_ALT "../resources/sirio_small.nnue"
#define SIRIO_SMALL_NETWORK_THRESHOLD 12

static sirio_nn_model g_eval_model;
static sirio_nn_model g_small_model;
static bool g_eval_initialized = false;
static bool g_use_nnue = true;

static int try_load_network(sirio_nn_model* model, const char* primary, const char* fallback) {
    if (sirio_nn_model_load(model, primary)) {
        return 1;
    }
    if (fallback && sirio_nn_model_load(model, fallback)) {
        return 1;
    }
    return 0;
}

static void eval_ensure_initialized(void) {
    if (g_eval_initialized) {
        return;
    }

    sirio_nn_model_init(&g_eval_model);
    sirio_nn_model_init(&g_small_model);

    if (!try_load_network(&g_eval_model, SIRIO_DEFAULT_NETWORK, SIRIO_DEFAULT_NETWORK_ALT)) {
        fprintf(stderr,
                "info: no NNUE weights loaded from %s; falling back to material until EvalFile is set\n",
                SIRIO_DEFAULT_NETWORK);
    }

    if (!try_load_network(&g_small_model, SIRIO_DEFAULT_SMALL_NETWORK, SIRIO_DEFAULT_SMALL_NETWORK_ALT)) {
        fprintf(stderr,
                "info: no secondary network loaded from %s; EvalFileSmall can be used to supply one\n",
                SIRIO_DEFAULT_SMALL_NETWORK);
        sirio_nn_model_free(&g_small_model);
        sirio_nn_model_init(&g_small_model);
    }

    g_eval_initialized = true;
}

void eval_init(void) {
    eval_ensure_initialized();
}

void eval_shutdown(void) {
    if (!g_eval_initialized) {
        return;
    }
    sirio_nn_model_free(&g_eval_model);
    sirio_nn_model_free(&g_small_model);
    g_eval_initialized = false;
}

int eval_load_network(const char* path) {
    eval_ensure_initialized();
    if (!path || !*path) {
        return 0;
    }
    return sirio_nn_model_load(&g_eval_model, path);
}

int eval_load_small_network(const char* path) {
    eval_ensure_initialized();
    if (!path || !*path) {
        sirio_nn_model_free(&g_small_model);
        sirio_nn_model_init(&g_small_model);
        return 1;
    }
    return sirio_nn_model_load(&g_small_model, path);
}

void eval_set_use_nnue(bool use_nnue) {
    g_use_nnue = use_nnue;
}

bool eval_use_nnue(void) {
    return g_use_nnue;
}

bool eval_has_small_network(void) {
    eval_ensure_initialized();
    return sirio_nn_model_ready(&g_small_model);
}

static int board_piece_count(const Board* board) {
    int total = 0;
    for (int color = COLOR_WHITE; color < COLOR_NB; ++color) {
        for (int pt = PIECE_PAWN; pt < PIECE_TYPE_NB; ++pt) {
            const Bitboard bb = board->bitboards[pt + PIECE_TYPE_NB * color];
            total += bits_popcount(bb);
        }
    }
    return total;
}

static const sirio_nn_model* select_model(const Board* board) {
    if (!g_use_nnue) {
        return NULL;
    }
    if (!sirio_nn_model_ready(&g_eval_model)) {
        return NULL;
    }
    if (sirio_nn_model_ready(&g_small_model)) {
        int pieces = board_piece_count(board);
        if (pieces <= SIRIO_SMALL_NETWORK_THRESHOLD) {
            return &g_small_model;
        }
    }
    return &g_eval_model;
}

Value eval_position(const Board* board) {
    if (board == NULL) {
        return VALUE_DRAW;
    }

    eval_ensure_initialized();

    const sirio_nn_model* model = select_model(board);
    int score;
    if (model) {
        score = sirio_nn_evaluate_board(model, board);
    } else {
        score = sirio_nn_evaluate_board(&g_eval_model, board);
    }

    if (board->side_to_move == COLOR_BLACK) {
        score = -score;
    }
    return score;
}

