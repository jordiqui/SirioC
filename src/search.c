#include "search.h"

#include "board.h"
#include "move.h"
#include "movepick.h"
#include "tb.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static uint64_t search_now_ms(void) {
    clock_t now = clock();
    return (uint64_t)((now * 1000) / CLOCKS_PER_SEC);
}

static uint64_t search_compute_nps(uint64_t nodes, uint64_t elapsed_ms) {
    if (elapsed_ms == 0) {
        return nodes;
    }
    return (nodes * 1000ULL) / elapsed_ms;
}

static void search_format_score(Value value, char* buffer, size_t buffer_size) {
    if (buffer_size == 0) {
        return;
    }
    const int mate_threshold = VALUE_MATE - 100;
    if (value >= mate_threshold || value <= -mate_threshold) {
        int mate_in = (VALUE_MATE - abs(value) + 1) / 2;
        if (mate_in < 1) {
            mate_in = 1;
        }
        if (value > 0) {
            snprintf(buffer, buffer_size, "mate %d", mate_in);
        } else {
            snprintf(buffer, buffer_size, "mate -%d", mate_in);
        }
    } else {
        snprintf(buffer, buffer_size, "cp %d", value);
    }
}

static void search_format_pv(const SearchContext* context,
                             size_t index,
                             char* buffer,
                             size_t buffer_size) {
    if (!context || index >= context->pv_count || buffer_size == 0) {
        if (buffer_size > 0) {
            buffer[0] = '\0';
        }
        return;
    }

    buffer[0] = '\0';
    size_t length = (size_t)context->pv_lengths[index];
    if (length == 0) {
        return;
    }

    size_t written = 0;
    for (size_t i = 0; i < length; ++i) {
        char move_buffer[16];
        move_to_uci(&context->pv_table[index][i], move_buffer, sizeof(move_buffer));
        if (move_buffer[0] == '\0') {
            snprintf(move_buffer, sizeof(move_buffer), "0000");
        }

        size_t move_len = strlen(move_buffer);
        if (written != 0) {
            if (written + 1 < buffer_size) {
                buffer[written++] = ' ';
            } else {
                break;
            }
        }

        if (written + move_len >= buffer_size) {
            size_t available = buffer_size - written - 1;
            if (available > 0) {
                memcpy(buffer + written, move_buffer, available);
                written += available;
            }
            break;
        }

        memcpy(buffer + written, move_buffer, move_len);
        written += move_len;
    }
    buffer[written < buffer_size ? written : buffer_size - 1] = '\0';
}

static void search_maybe_emit_info(SearchContext* context, int depth, int force) {
    if (context == NULL) {
        return;
    }

    uint64_t now = search_now_ms();
    if (!force && context->last_info_report_ms != 0 &&
        now - context->last_info_report_ms < 200ULL) {
        return;
    }

    context->last_info_report_ms = now;

    uint64_t elapsed = now - context->start_time_ms;
    if (elapsed == 0) {
        elapsed = 1;
    }

    if (context->pv_count == 0) {
        return;
    }

    if (depth <= 0) {
        depth = 1;
    }

    uint64_t nps = search_compute_nps(context->nodes, elapsed);
    int hashfull = transposition_hashfull(context->tt);
    uint64_t tb_hits = tb_get_hits();
    int seldepth = context->seldepth > 0 ? context->seldepth : depth;

    for (size_t i = 0; i < context->pv_count; ++i) {
        char score_buffer[32];
        char pv_buffer[512];
        search_format_score(context->pv_values[i], score_buffer, sizeof(score_buffer));
        search_format_pv(context, i, pv_buffer, sizeof(pv_buffer));
        if (pv_buffer[0] == '\0') {
            snprintf(pv_buffer, sizeof(pv_buffer), "0000");
        }

        printf("info depth %d seldepth %d multipv %zu score %s nodes %" PRIu64
               " nps %" PRIu64 " hashfull %d tbhits %" PRIu64 " time %" PRIu64
               " pv %s\n",
               depth,
               seldepth,
               i + 1,
               score_buffer,
               context->nodes,
               nps,
               hashfull,
               tb_hits,
               elapsed,
               pv_buffer);
    }
    fflush(stdout);
}

static int search_should_stop(SearchContext* context) {
    if (context == NULL) {
        return 1;
    }
    if (context->stop) {
        return 1;
    }
    int report_depth = context->depth_completed > 0 ? context->depth_completed : 1;
    search_maybe_emit_info(context, report_depth, 0);
    if (context->limits.infinite) {
        return 0;
    }
    if (context->limits.nodes > 0 && context->nodes >= (uint64_t)context->limits.nodes) {
        context->stop = 1;
        search_maybe_emit_info(context, report_depth, 1);
        return 1;
    }
    if (context->limits.movetime_ms > 0) {
        uint64_t elapsed = search_now_ms() - context->start_time_ms;
        if (elapsed >= (uint64_t)context->limits.movetime_ms) {
            context->stop = 1;
            search_maybe_emit_info(context, report_depth, 1);
            return 1;
        }
    }
    return 0;
}

static Value search_alpha_beta(SearchContext* context,
                              int depth,
                              int ply,
                              Value alpha,
                              Value beta,
                              Move* pv_line,
                              int* pv_length) {
    if (context == NULL) {
        return VALUE_NONE;
    }
    context->nodes++;
    if (search_should_stop(context)) {
        if (pv_length) {
            *pv_length = 0;
        }
        return eval_position(context->board);
    }
    if (depth <= 0) {
        if (pv_length) {
            *pv_length = 0;
        }
        return eval_position(context->board);
    }

    const uint64_t position_key = context->board ? context->board->zobrist_key : 0ULL;
    Value original_alpha = alpha;
    Value original_beta = beta;

    if (ply > context->seldepth) {
        context->seldepth = ply;
    }

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
        if (pv_length) {
            *pv_length = 0;
        }
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
    Move best_line[64];
    memset(best_line, 0, sizeof(best_line));
    int best_length = 0;
    for (size_t i = 0; i < moves.size; ++i) {
        Move move = moves.moves[i];
        Move child_line[64];
        int child_length = 0;
        memset(child_line, 0, sizeof(child_line));
        board_make_move(context->board, &move);
        Value score = -search_alpha_beta(context, depth - 1, ply + 1, -beta, -alpha, child_line, &child_length);
        board_unmake_move(context->board, &move);

        if (score > best) {
            best = score;
            best_move = move;
            best_length = child_length;
            if (best_length > 0) {
                memcpy(best_line, child_line, (size_t)best_length * sizeof(Move));
            }
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

    if (pv_length) {
        if (!move_is_null(&best_move)) {
            pv_line[0] = best_move;
            if (best_length > 0) {
                memcpy(pv_line + 1, best_line, (size_t)best_length * sizeof(Move));
            }
            *pv_length = best_length + 1;
        } else {
            *pv_length = 0;
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
    context->seldepth = 0;
    context->last_search_time_ms = 0;
    context->last_info_report_ms = 0;
    memset(context->pv_lengths, 0, sizeof(context->pv_lengths));
    memset(context->pv_table, 0, sizeof(context->pv_table));
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
    memset(context->pv_lengths, 0, sizeof(context->pv_lengths));
    context->multipv = limits->multipv > 0 ? limits->multipv : 1;
    context->start_time_ms = search_now_ms();
    context->nodes = 0;
    context->depth_completed = 0;
    context->seldepth = 0;
    context->last_search_time_ms = 0;
    context->last_info_report_ms = context->start_time_ms;

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
        if (context->seldepth < depth) {
            context->seldepth = depth;
        }
        search_maybe_emit_info(context, depth, 1);
    }

    context->best_value = best_value;
    context->last_search_time_ms = search_now_ms() - context->start_time_ms;
    if (context->stop && context->depth_completed > 0) {
        search_maybe_emit_info(context, context->depth_completed, 1);
    }

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
            context->pv_lengths[0] = 1;
            context->pv_table[0][0] = tb_move;
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
        Move pv[64];
        int pv_length;
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
        Move child_line[64];
        int child_length = 0;
        board_make_move(context->board, &move);
        Value score = -search_alpha_beta(context, depth - 1, 1, -beta, -alpha, child_line, &child_length);
        board_unmake_move(context->board, &move);

        if (entry_count < 256) {
            entries[entry_count].move = move;
            entries[entry_count].value = score;
            entries[entry_count].pv_length = child_length + 1;
            entries[entry_count].pv[0] = move;
            if (child_length > 0) {
                memcpy(entries[entry_count].pv + 1, child_line, (size_t)child_length * sizeof(Move));
            }
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
        context->pv_lengths[i] = entries[i].pv_length;
        if (entries[i].pv_length > 0) {
            memcpy(context->pv_table[i], entries[i].pv, (size_t)entries[i].pv_length * sizeof(Move));
        } else {
            memset(context->pv_table[i], 0, sizeof(context->pv_table[i]));
        }
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

