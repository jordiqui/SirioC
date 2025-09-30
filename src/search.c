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
    if (context->limits.nodes > 0 && context->nodes >= (uint64_t)context->limits.nodes) {
        context->stop = 1;
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
    context->nodes++;
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
    context->move_overhead = 10;
    context->nodes = 0;
    context->depth_completed = 0;
    context->last_search_time_ms = 0;
    (void)tt;
}

static int search_max_depth(const SearchLimits* limits) {
    if (limits == NULL || limits->depth <= 0) {
        return 64;
    }
    return limits->depth;
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
    context->nodes = 0;
    context->depth_completed = 0;
    context->last_search_time_ms = 0;

    Move best_move = move_create(0, 0, PIECE_NONE, PIECE_NONE, PIECE_NONE, 0);
    Value aspiration_center = 0;
    Value best_value = 0;
    const int max_depth = search_max_depth(limits);
    for (int depth = 1; depth <= max_depth; ++depth) {
        Value alpha = -VALUE_INFINITE;
        Value beta = VALUE_INFINITE;
        if (depth > 1) {
            const Value aspiration_window = 50;
            alpha = aspiration_center - aspiration_window;
            beta = aspiration_center + aspiration_window;
            if (alpha < -VALUE_INFINITE) {
                alpha = -VALUE_INFINITE;
            }
            if (beta > VALUE_INFINITE) {
                beta = VALUE_INFINITE;
            }
        }

        Value value = search_root(context, depth, alpha, beta);
        while (!context->stop && depth > 1 && (value <= alpha || value >= beta)) {
            if (value <= alpha) {
                alpha = (Value)(alpha - 2 * 50);
                if (alpha < -VALUE_INFINITE) {
                    alpha = -VALUE_INFINITE;
                }
            } else if (value >= beta) {
                beta = (Value)(beta + 2 * 50);
                if (beta > VALUE_INFINITE) {
                    beta = VALUE_INFINITE;
                }
            }
            value = search_root(context, depth, alpha, beta);
        }

        if (context->stop) {
            break;
        }

        if (!move_is_null(&context->best_move)) {
            best_move = context->best_move;
            best_value = value;
            aspiration_center = value;
        }

        context->depth_completed = depth;
    }

    context->best_value = best_value;
    context->last_search_time_ms = search_now_ms() - context->start_time_ms;

    return best_move;
}

Value search_root(SearchContext* context, int depth, Value alpha, Value beta) {
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

    Value original_alpha = alpha;
    Value original_beta = beta;

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
        Value score = -search_alpha_beta(context, depth - 1, -beta, -alpha);
        board_unmake_move(context->board, &move);

        if (entry_count < 256) {
            entries[entry_count].move = move;
            entries[entry_count].value = score;
            ++entry_count;
        }

        if (move_is_null(&best_move) || score > best_score) {
            best_move = move;
            best_score = score;
            if (score > alpha) {
                alpha = score;
            }
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

    if (best_score <= original_alpha) {
        return original_alpha;
    }
    if (best_score >= original_beta) {
        return original_beta;
    }
    return best_score;
}

