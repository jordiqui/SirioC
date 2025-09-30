#include "search.h"

#include "board.h"
#include "move.h"
#include "movepick.h"

#include <time.h>

static uint64_t search_now_ms(void) {
    clock_t now = clock();
    return (uint64_t)((now * 1000) / CLOCKS_PER_SEC);
}

static int search_should_stop(SearchContext* context) {
    if (context == NULL) {
        return 1;
    }
    if (context->limits.infinite) {
        return 0;
    }
    if (context->stop) {
        return 1;
    }
    if (context->limits.movetime_ms > 0) {
        uint64_t elapsed = search_now_ms() - context->start_time_ms;
        if (elapsed >= (uint64_t)context->limits.movetime_ms) {
            context->stop = 1;
            return 1;
        }
    }
    return 0;
}

static Value search_alpha_beta(SearchContext* context, int depth, Value alpha, Value beta) {
    if (context == NULL) {
        return VALUE_NONE;
    }
    if (search_should_stop(context)) {
        return eval_position(context->board);
    }
    if (depth <= 0) {
        return eval_position(context->board);
    }

    MoveList moves;
    movegen_generate_legal_moves(context->board, &moves);
    if (moves.size == 0) {
        return eval_position(context->board);
    }

    movepick_sort(&moves);

    Value best = -VALUE_INFINITE;
    for (size_t i = 0; i < moves.size; ++i) {
        Move move = moves.moves[i];
        board_make_move(context->board, &move);
        Value score = -search_alpha_beta(context, depth - 1, -beta, -alpha);
        board_unmake_move(context->board, &move);

        if (score > best) {
            best = score;
        }
        if (score > alpha) {
            alpha = score;
        }
        if (alpha >= beta) {
            break;
        }
        if (search_should_stop(context)) {
            break;
        }
    }

    return best;
}

void search_init(SearchContext* context, Board* board, TranspositionTable* tt, HistoryTable* history) {
    if (context == NULL) {
        return;
    }
    context->board = board;
    context->history = history;
    context->best_value = VALUE_NONE;
    context->best_move = move_create(0, 0, PIECE_NONE, PIECE_NONE, PIECE_NONE, 0);
    context->pv_count = 0;
    context->multipv = 1;
    context->start_time_ms = 0;
    context->stop = 0;
    (void)tt;
}

Move search_iterative_deepening(SearchContext* context, const SearchLimits* limits) {
    if (context == NULL || limits == NULL) {
        return move_create(0, 0, PIECE_NONE, PIECE_NONE, PIECE_NONE, 0);
    }

    context->limits = *limits;
    context->stop = 0;
    context->pv_count = 0;
    context->multipv = limits->multipv > 0 ? limits->multipv : 1;
    context->start_time_ms = search_now_ms();

    Move best_move = move_create(0, 0, PIECE_NONE, PIECE_NONE, PIECE_NONE, 0);
    for (int depth = 1; depth <= limits->depth; ++depth) {
        Value value = search_root(context, depth);
        if (!move_is_null(&context->best_move)) {
            best_move = context->best_move;
            context->best_value = value;
        }
        if (search_should_stop(context)) {
            break;
        }
    }

    return best_move;
}

Value search_root(SearchContext* context, int depth) {
    if (context == NULL) {
        return VALUE_NONE;
    }
    MoveList moves;
    movegen_generate_legal_moves(context->board, &moves);
    context->pv_count = 0;

    if (moves.size == 0) {
        context->best_move = move_create(0, 0, PIECE_NONE, PIECE_NONE, PIECE_NONE, 0);
        context->best_value = eval_position(context->board);
        return context->best_value;
    }

    typedef struct {
        Move move;
        Value value;
    } RootEntry;

    RootEntry entries[256];
    size_t entry_count = 0;

    Value best_score = -VALUE_INFINITE;
    Move best_move = move_create(0, 0, PIECE_NONE, PIECE_NONE, PIECE_NONE, 0);

    for (size_t i = 0; i < moves.size; ++i) {
        if (search_should_stop(context)) {
            break;
        }
        Move move = moves.moves[i];
        board_make_move(context->board, &move);
        Value score = -search_alpha_beta(context, depth - 1, -VALUE_INFINITE, VALUE_INFINITE);
        board_unmake_move(context->board, &move);

        if (entry_count < 256) {
            entries[entry_count].move = move;
            entries[entry_count].value = score;
            ++entry_count;
        }

        if (move_is_null(&best_move) || score > best_score) {
            best_move = move;
            best_score = score;
        }
    }

    for (size_t i = 0; i < entry_count; ++i) {
        for (size_t j = i + 1; j < entry_count; ++j) {
            if (entries[j].value > entries[i].value) {
                RootEntry temp = entries[i];
                entries[i] = entries[j];
                entries[j] = temp;
            }
        }
    }

    size_t wanted = (size_t)context->multipv;
    if (wanted == 0) {
        wanted = 1;
    }
    if (wanted > entry_count) {
        wanted = entry_count;
    }
    context->pv_count = wanted;
    for (size_t i = 0; i < wanted; ++i) {
        context->pv_moves[i] = entries[i].move;
        context->pv_values[i] = entries[i].value;
    }

    context->best_move = best_move;
    context->best_value = best_score;
    return best_score;
}

