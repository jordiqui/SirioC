#include "search.h"

#include "board.h"
#include "move.h"
#include "movepick.h"
#include "tb.h"

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

    const uint64_t position_key = context->board ? context->board->zobrist_key : 0ULL;
    Value original_alpha = alpha;
    Value original_beta = beta;

    if (depth >= tb_get_probe_depth()) {
        Value tb_value = tb_probe(context->board);
        if (tb_value != VALUE_NONE) {
            if (context->tt) {
                Move null_move = move_create(0, 0, PIECE_NONE, PIECE_NONE, PIECE_NONE, 0);
                transposition_store(context->tt, position_key, tb_value, null_move, depth, TT_FLAG_EXACT);
            }
            return tb_value;
        }
    }

    const TranspositionEntry* entry = NULL;
    if (context->tt) {
        entry = transposition_probe(context->tt, position_key);
        if (entry && entry->depth >= depth) {
            if (entry->flags == TT_FLAG_EXACT) {
                return entry->value;
            }
            if (entry->flags == TT_FLAG_LOWER && entry->value > alpha) {
                alpha = entry->value;
            } else if (entry->flags == TT_FLAG_UPPER && entry->value < beta) {
                beta = entry->value;
            }
            if (alpha >= beta) {
                return entry->value;
            }
        }
    }

    MoveList moves;
    movegen_generate_legal_moves(context->board, &moves);
    if (moves.size == 0) {
        return eval_position(context->board);
    }

    if (entry && !move_is_null(&entry->best_move)) {
        for (size_t i = 0; i < moves.size; ++i) {
            Move* current = &moves.moves[i];
            if (current->from == entry->best_move.from && current->to == entry->best_move.to &&
                current->piece == entry->best_move.piece && current->promotion == entry->best_move.promotion) {
                if (i != 0) {
                    Move temp = moves.moves[0];
                    moves.moves[0] = *current;
                    *current = temp;
                }
                break;
            }
        }
    }

    movepick_sort(&moves);

    Value best = -VALUE_INFINITE;
    Move best_move = move_create(0, 0, PIECE_NONE, PIECE_NONE, PIECE_NONE, 0);
    for (size_t i = 0; i < moves.size; ++i) {
        Move move = moves.moves[i];
        board_make_move(context->board, &move);
        Value score = -search_alpha_beta(context, depth - 1, -beta, -alpha);
        board_unmake_move(context->board, &move);

        if (score > best) {
            best = score;
            best_move = move;
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

    if (context->tt) {
        int flag = TT_FLAG_EXACT;
        if (best <= original_alpha) {
            flag = TT_FLAG_UPPER;
        } else if (best >= original_beta) {
            flag = TT_FLAG_LOWER;
        }
        transposition_store(context->tt, position_key, best, best_move, depth, flag);
    }

    return best;
}

void search_init(SearchContext* context, Board* board, TranspositionTable* tt, HistoryTable* history) {
    if (context == NULL) {
        return;
    }
    context->board = board;
    context->history = history;
    context->tt = tt;
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

    const uint64_t position_key = context->board ? context->board->zobrist_key : 0ULL;

    if (depth >= tb_get_probe_depth()) {
        Move tb_move = move_create(0, 0, PIECE_NONE, PIECE_NONE, PIECE_NONE, 0);
        Value tb_value = VALUE_NONE;
        if (tb_probe_root_position(context->board, &tb_move, &tb_value)) {
            context->best_move = tb_move;
            context->best_value = tb_value;
            context->pv_count = 1;
            context->pv_moves[0] = tb_move;
            context->pv_values[0] = tb_value;
            if (context->tt) {
                transposition_store(context->tt, position_key, tb_value, tb_move, depth, TT_FLAG_EXACT);
            }
            return tb_value;
        }
    }

    Value original_alpha = alpha;
    Value original_beta = beta;

    typedef struct {
        Move move;
        Value value;
    } RootEntry;

    RootEntry entries[256];
    size_t entry_count = 0;

    const TranspositionEntry* tt_entry = context->tt ? transposition_probe(context->tt, position_key) : NULL;
    if (tt_entry && !move_is_null(&tt_entry->best_move)) {
        for (size_t i = 0; i < moves.size; ++i) {
            Move* current = &moves.moves[i];
            if (current->from == tt_entry->best_move.from && current->to == tt_entry->best_move.to &&
                current->piece == tt_entry->best_move.piece && current->promotion == tt_entry->best_move.promotion) {
                if (i != 0) {
                    Move temp = moves.moves[0];
                    moves.moves[0] = *current;
                    *current = temp;
                }
                break;
            }
        }
    }

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

    if (context->tt) {
        int flag = TT_FLAG_EXACT;
        if (best_score <= original_alpha) {
            flag = TT_FLAG_UPPER;
        } else if (best_score >= original_beta) {
            flag = TT_FLAG_LOWER;
        }
        transposition_store(context->tt, position_key, best_score, best_move, depth, flag);
    }

    if (best_score <= original_alpha) {
        return original_alpha;
    }
    if (best_score >= original_beta) {
        return original_beta;
    }
    return best_score;
}

