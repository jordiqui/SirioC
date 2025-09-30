#include "search.h"

#include "board.h"
#include "move.h"
#include "movepick.h"

static Value search_alpha_beta(SearchContext* context, int depth, Value alpha, Value beta) {
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
            context->best_move = move;
            context->best_value = score;
        }
        if (alpha >= beta) {
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
    (void)tt;
}

Move search_iterative_deepening(SearchContext* context, const SearchLimits* limits) {
    if (context == NULL || limits == NULL) {
        return move_create(0, 0, PIECE_NONE, PIECE_NONE, PIECE_NONE, 0);
    }

    Move best_move = move_create(0, 0, PIECE_NONE, PIECE_NONE, PIECE_NONE, 0);
    for (int depth = 1; depth <= limits->depth; ++depth) {
        Value value = search_root(context, depth);
        if (!move_is_null(&context->best_move)) {
            best_move = context->best_move;
            context->best_value = value;
        }
    }

    return best_move;
}

Value search_root(SearchContext* context, int depth) {
    if (context == NULL) {
        return VALUE_NONE;
    }
    return search_alpha_beta(context, depth, -VALUE_INFINITE, VALUE_INFINITE);
}

