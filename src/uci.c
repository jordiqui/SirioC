#include "uci.h"

#include <ctype.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bench.h"
#include "board.h"
#include "eval.h"
#include "history.h"
#include "move.h"
#include "movegen.h"
#include "search.h"
#include "transposition.h"
#include "tb.h"

#define UCI_DEFAULT_EVAL_FILE "resources/nn-1c0000000000.nnue"
#define UCI_DEFAULT_EVAL_FILE_SMALL "resources/nn-37f18f62d772.nnue"

typedef struct {
    SearchContext* context;
    char eval_file[512];
    char eval_file_small[512];
    char syzygy_path[1024];
    int syzygy_probe_depth;
    int syzygy_50_move_rule;
    int syzygy_probe_limit;
    int move_overhead;
    int hash_size_mb;
    int threads;
    int ponder_enabled;
    int search_thread_running;
    int search_thread_active;
    pthread_t search_thread;
} UciState;

static void trim_whitespace(char* text) {
    if (!text) {
        return;
    }

    char* end = text + strlen(text);
    while (end > text && isspace((unsigned char)*(end - 1))) {
        --end;
    }
    *end = '\0';

    char* start = text;
    while (*start && isspace((unsigned char)*start)) {
        ++start;
    }
    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }
}

static void print_options(const UciState* state) {
    printf("option name UseNNUE type check default %s\n", eval_use_nnue() ? "true" : "false");
    printf("option name EvalFile type string default %s\n", state->eval_file);
    const char* small_value = state->eval_file_small[0] ? state->eval_file_small : "<empty>";
    printf("option name EvalFileSmall type string default %s\n", small_value);
    int multipv = 1;
    if (state && state->context && state->context->multipv > 0) {
        multipv = state->context->multipv;
    }
    printf("option name MultiPV type spin default %d min 1 max 256\n", multipv);
    printf("option name Ponder type check default %s\n", state->ponder_enabled ? "true" : "false");
    int threads = state ? state->threads : 1;
    if (threads <= 0) {
        threads = 1;
    }
    printf("option name Threads type spin default %d min 1 max 256\n", threads);
    int hash_mb = state ? state->hash_size_mb : 64;
    if (hash_mb <= 0) {
        hash_mb = 64;
    }
    printf("option name Hash type spin default %d min 1 max 4096\n", hash_mb);
    printf("option name SyzygyPath type string default %s\n", state->syzygy_path);
    printf("option name SyzygyProbeDepth type spin default %d min 0 max 64\n", state->syzygy_probe_depth);
    printf("option name Syzygy50MoveRule type check default %s\n", state->syzygy_50_move_rule ? "true" : "false");
    printf("option name SyzygyProbeLimit type spin default %d min 0 max 7\n", state->syzygy_probe_limit);
    printf("option name Move Overhead type spin default %d min 0 max 5000\n", state->move_overhead);
}

static char* next_token(char** cursor) {
    if (cursor == NULL || *cursor == NULL) {
        return NULL;
    }

    char* text = *cursor;
    while (*text && isspace((unsigned char)*text)) {
        ++text;
    }
    if (*text == '\0') {
        *cursor = text;
        return NULL;
    }

    char* start = text;
    while (*text && !isspace((unsigned char)*text)) {
        ++text;
    }
    if (*text) {
        *text = '\0';
        ++text;
    }
    *cursor = text;
    return start;
}

static int parse_numeric_token(char** cursor) {
    char* token = next_token(cursor);
    if (!token) {
        return 0;
    }
    return atoi(token);
}

static uint64_t compute_nps(uint64_t nodes, uint64_t elapsed_ms) {
    if (elapsed_ms == 0) {
        return nodes;
    }
    return (nodes * 1000ULL) / elapsed_ms;
}

static void emit_multipv_info(const UciState* state);

static void uci_format_score(Value value, char* buffer, size_t size) {
    if (size == 0) {
        return;
    }
    const int mate_threshold = VALUE_MATE - 100;
    if (value >= mate_threshold || value <= -mate_threshold) {
        int mate_in = (VALUE_MATE - abs(value) + 1) / 2;
        if (mate_in < 1) {
            mate_in = 1;
        }
        if (value > 0) {
            snprintf(buffer, size, "mate %d", mate_in);
        } else {
            snprintf(buffer, size, "mate -%d", mate_in);
        }
    } else {
        snprintf(buffer, size, "cp %d", value);
    }
}

static void uci_format_pv(const SearchContext* context, size_t index, char* buffer, size_t size) {
    if (!context || index >= context->pv_count || size == 0) {
        if (size > 0) {
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
            if (written + 1 < size) {
                buffer[written++] = ' ';
            } else {
                break;
            }
        }
        if (written + move_len >= size) {
            size_t available = size - written - 1;
            if (available > 0) {
                memcpy(buffer + written, move_buffer, available);
                written += available;
            }
            break;
        }
        memcpy(buffer + written, move_buffer, move_len);
        written += move_len;
    }
    buffer[written < size ? written : size - 1] = '\0';
}

typedef struct {
    UciState* state;
    SearchLimits limits;
} UciSearchThreadData;

static void stop_search(UciState* state) {
    if (!state) {
        return;
    }

    if (state->context) {
        state->context->stop = 1;
    }

    if (state->search_thread_active) {
        pthread_join(state->search_thread, NULL);
        state->search_thread_active = 0;
        state->search_thread_running = 0;
    }
}

static void* uci_search_thread(void* arg) {
    UciSearchThreadData* data = (UciSearchThreadData*)arg;
    if (!data) {
        return NULL;
    }

    UciState* state = data->state;
    SearchLimits limits = data->limits;
    free(data);

    if (!state || !state->context) {
        if (state) {
            state->search_thread_running = 0;
        }
        return NULL;
    }

    Move best = search_iterative_deepening(state->context, &limits);
    emit_multipv_info(state);

    char buffer[16];
    move_to_uci(&best, buffer, sizeof(buffer));
    if (buffer[0] == '\0') {
        snprintf(buffer, sizeof(buffer), "0000");
    }

    printf("bestmove %s\n", buffer);
    fflush(stdout);

    state->search_thread_running = 0;
    return NULL;
}

static void emit_multipv_info(const UciState* state) {
    if (!state || !state->context) {
        return;
    }

    const SearchContext* context = state->context;
    uint64_t elapsed = context->last_search_time_ms;
    if (elapsed == 0) {
        elapsed = 1;
    }
    const uint64_t nodes = context->nodes;
    const uint64_t nps = compute_nps(nodes, elapsed);
    int depth = context->depth_completed > 0 ? context->depth_completed : context->limits.depth;
    if (depth <= 0) {
        depth = 1;
    }
    int seldepth = context->seldepth > 0 ? context->seldepth : depth;
    int hashfull = transposition_hashfull(context->tt);
    uint64_t tb_hits = tb_get_hits();

    if (context->pv_count == 0) {
        return;
    }

    for (size_t index = 0; index < context->pv_count; ++index) {
        char score_buffer[32];
        char pv_buffer[512];
        uci_format_score(context->pv_values[index], score_buffer, sizeof(score_buffer));
        uci_format_pv(context, index, pv_buffer, sizeof(pv_buffer));
        if (pv_buffer[0] == '\0') {
            snprintf(pv_buffer, sizeof(pv_buffer), "0000");
        }
        printf("info depth %d seldepth %d multipv %zu score %s nodes %" PRIu64
               " nps %" PRIu64 " hashfull %d tbhits %" PRIu64 " time %" PRIu64
               " pv %s\n",
               depth,
               seldepth,
               index + 1,
               score_buffer,
               nodes,
               nps,
               hashfull,
               tb_hits,
               elapsed,
               pv_buffer);
    }
    fflush(stdout);
}

static void handle_position(UciState* state, char* args) {
    (void)state;
    if (!args) {
        return;
    }

    while (*args == ' ') {
        ++args;
    }

    if (strncmp(args, "startpos", 8) == 0) {
        board_set_start_position(state->context->board);
        args += 8;
    } else {
        board_set_start_position(state->context->board);
    }

    char* moves = strstr(args, "moves");
    if (moves) {
        moves += 5;
        while (*moves == ' ') {
            ++moves;
        }
        while (*moves) {
            char move_str[16] = {0};
            int idx = 0;
            while (*moves && !isspace((unsigned char)*moves) && idx < (int)sizeof(move_str) - 1) {
                move_str[idx++] = *moves++;
            }
            move_str[idx] = '\0';
            while (isspace((unsigned char)*moves)) {
                ++moves;
            }

            if (idx == 0) {
                break;
            }

            MoveList list;
            movegen_generate_legal_moves(state->context->board, &list);
            Move selected = move_create(0, 0, PIECE_NONE, PIECE_NONE, PIECE_NONE, 0);
            for (size_t i = 0; i < list.size; ++i) {
                char buffer[16];
                move_to_uci(&list.moves[i], buffer, sizeof(buffer));
                if (strcmp(buffer, move_str) == 0) {
                    selected = list.moves[i];
                    break;
                }
            }

            if (move_is_null(&selected)) {
                break;
            }

            board_make_move(state->context->board, &selected);
        }
    }
}

static void handle_setoption(UciState* state, char* line) {
    if (!line) {
        return;
    }

    char* name_ptr = strstr(line, "name");
    if (!name_ptr) {
        return;
    }
    name_ptr += 4;
    while (*name_ptr == ' ') {
        ++name_ptr;
    }

    char* value_ptr = strstr(line, "value");
    char* option_value = NULL;
    if (value_ptr) {
        char* name_end = value_ptr;
        while (name_end > line && isspace((unsigned char)*(name_end - 1))) {
            --name_end;
        }
        *name_end = '\0';
        value_ptr += 5;
        while (*value_ptr == ' ') {
            ++value_ptr;
        }
        option_value = value_ptr;
        trim_whitespace(option_value);
    }

    trim_whitespace(name_ptr);

    if (strcmp(name_ptr, "UseNNUE") == 0 && option_value) {
        if (strcmp(option_value, "true") == 0 || strcmp(option_value, "1") == 0) {
            eval_set_use_nnue(true);
        } else {
            eval_set_use_nnue(false);
        }
    } else if (strcmp(name_ptr, "EvalFile") == 0 && option_value) {
        if (eval_load_network(option_value)) {
            snprintf(state->eval_file, sizeof(state->eval_file), "%s", option_value);
        } else {
            printf("info string Failed to load NNUE file %s\n", option_value);
        }
    } else if (strcmp(name_ptr, "EvalFileSmall") == 0) {
        if (option_value && *option_value) {
            if (eval_load_small_network(option_value)) {
                snprintf(state->eval_file_small, sizeof(state->eval_file_small), "%s", option_value);
            } else {
                printf("info string Failed to load secondary NNUE file %s\n", option_value);
            }
        } else {
            eval_load_small_network(NULL);
            state->eval_file_small[0] = '\0';
        }
    } else if (strcmp(name_ptr, "MultiPV") == 0 && option_value) {
        if (state && state->context) {
            int multipv = atoi(option_value);
            if (multipv < 1) {
                multipv = 1;
            } else if (multipv > 256) {
                multipv = 256;
            }
            state->context->multipv = multipv;
        }
    } else if (strcmp(name_ptr, "Ponder") == 0 && option_value) {
        if (strcmp(option_value, "true") == 0 || strcmp(option_value, "1") == 0) {
            state->ponder_enabled = 1;
        } else {
            state->ponder_enabled = 0;
        }
    } else if (strcmp(name_ptr, "Threads") == 0 && option_value) {
        int threads = atoi(option_value);
        if (threads < 1) {
            threads = 1;
        } else if (threads > 256) {
            threads = 256;
        }
        state->threads = threads;
    } else if (strcmp(name_ptr, "Hash") == 0 && option_value) {
        int mb = atoi(option_value);
        if (mb < 1) {
            mb = 1;
        } else if (mb > 4096) {
            mb = 4096;
        }
        state->hash_size_mb = mb;
        if (state->context && state->context->tt) {
            stop_search(state);
            size_t bytes = (size_t)mb * 1024ULL * 1024ULL;
            size_t entry_size = sizeof(TranspositionEntry);
            size_t entries = bytes / entry_size;
            if (entries < 1024) {
                entries = 1024;
            }
            transposition_free(state->context->tt);
            transposition_init(state->context->tt, entries);
        }
    } else if (strcmp(name_ptr, "SyzygyPath") == 0 && option_value) {
        snprintf(state->syzygy_path, sizeof(state->syzygy_path), "%s", option_value);
        tb_set_path(option_value);
    } else if (strcmp(name_ptr, "SyzygyProbeDepth") == 0 && option_value) {
        int depth = atoi(option_value);
        if (depth < 0) {
            depth = 0;
        } else if (depth > 64) {
            depth = 64;
        }
        state->syzygy_probe_depth = depth;
        tb_set_probe_depth(depth);
    } else if (strcmp(name_ptr, "Syzygy50MoveRule") == 0 && option_value) {
        int enabled = (strcmp(option_value, "true") == 0 || strcmp(option_value, "1") == 0);
        state->syzygy_50_move_rule = enabled;
        tb_set_50_move_rule(enabled);
    } else if (strcmp(name_ptr, "SyzygyProbeLimit") == 0 && option_value) {
        int limit = atoi(option_value);
        if (limit < 0) {
            limit = 0;
        } else if (limit > 7) {
            limit = 7;
        }
        state->syzygy_probe_limit = limit;
        tb_set_probe_limit(limit);
    } else if (strcmp(name_ptr, "Move Overhead") == 0 && option_value) {
        int overhead = atoi(option_value);
        if (overhead < 0) {
            overhead = 0;
        } else if (overhead > 5000) {
            overhead = 5000;
        }
        state->move_overhead = overhead;
        if (state->context) {
            state->context->move_overhead = overhead;
        }
    }
}

static void handle_go(UciState* state, char* args) {
    if (!state || !state->context) {
        return;
    }

    SearchLimits limits = { .depth = 0,
                            .movetime_ms = 0,
                            .nodes = 0,
                            .infinite = 0,
                            .multipv = state->context->multipv > 0 ? state->context->multipv : 1,
                            .wtime_ms = 0,
                            .btime_ms = 0,
                            .winc_ms = 0,
                            .binc_ms = 0,
                            .moves_to_go = 0,
                            .ponder = state->ponder_enabled };

    if (args) {
        char* cursor = args;
        char* token = NULL;
        while ((token = next_token(&cursor)) != NULL) {
            if (strcmp(token, "depth") == 0) {
                limits.depth = parse_numeric_token(&cursor);
            } else if (strcmp(token, "movetime") == 0) {
                limits.movetime_ms = parse_numeric_token(&cursor);
            } else if (strcmp(token, "nodes") == 0) {
                limits.nodes = parse_numeric_token(&cursor);
            } else if (strcmp(token, "wtime") == 0) {
                limits.wtime_ms = parse_numeric_token(&cursor);
            } else if (strcmp(token, "btime") == 0) {
                limits.btime_ms = parse_numeric_token(&cursor);
            } else if (strcmp(token, "winc") == 0) {
                limits.winc_ms = parse_numeric_token(&cursor);
            } else if (strcmp(token, "binc") == 0) {
                limits.binc_ms = parse_numeric_token(&cursor);
            } else if (strcmp(token, "movestogo") == 0) {
                limits.moves_to_go = parse_numeric_token(&cursor);
            } else if (strcmp(token, "infinite") == 0) {
                limits.infinite = 1;
            } else if (strcmp(token, "ponder") == 0) {
                limits.ponder = 1;
            } else if (strcmp(token, "multipv") == 0) {
                limits.multipv = parse_numeric_token(&cursor);
            } else if (strcmp(token, "searchmoves") == 0) {
                /* searchmoves not yet supported; skip remaining tokens on the line */
                while ((token = next_token(&cursor)) != NULL) {
                    (void)token;
                }
                break;
            }
        }
    }

    if (limits.multipv <= 0) {
        limits.multipv = 1;
    }

    if (!limits.infinite && limits.movetime_ms > 0) {
        int overhead = state->context->move_overhead;
        if (limits.movetime_ms > overhead) {
            limits.movetime_ms -= overhead;
        } else {
            limits.movetime_ms = 1;
        }
    }

    if (limits.depth <= 0 && limits.movetime_ms <= 0 && limits.nodes <= 0 && !limits.infinite &&
        limits.wtime_ms <= 0 && limits.btime_ms <= 0) {
        limits.depth = 64;
    }

    if (!limits.infinite && limits.movetime_ms <= 0) {
        int moves_to_go = limits.moves_to_go > 0 ? limits.moves_to_go : 30;
        if (moves_to_go <= 0) {
            moves_to_go = 30;
        }

        enum Color color = state->context->board ? state->context->board->side_to_move : COLOR_WHITE;
        int remaining_time = (color == COLOR_WHITE) ? limits.wtime_ms : limits.btime_ms;
        int increment = (color == COLOR_WHITE) ? limits.winc_ms : limits.binc_ms;

        if (remaining_time > 0) {
            long budget = remaining_time / moves_to_go;
            if (budget < 0) {
                budget = 0;
            }
            if (increment > 0) {
                budget += increment;
            }
            if (budget > remaining_time) {
                budget = remaining_time;
            }

            int overhead = state->context->move_overhead;
            if (overhead > 0 && budget > overhead) {
                budget -= overhead;
            }
            if (budget <= 0) {
                budget = remaining_time / (moves_to_go > 1 ? moves_to_go : 2);
            }
            if (budget <= 0) {
                budget = remaining_time;
            }
            if (budget <= 0) {
                budget = 1;
            }
            limits.movetime_ms = (int)budget;
        }
    }

    if (limits.movetime_ms < 0) {
        limits.movetime_ms = 0;
    }

    stop_search(state);
    state->context->stop = 0;

    UciSearchThreadData* data = (UciSearchThreadData*)malloc(sizeof(UciSearchThreadData));
    if (!data) {
        Move best = search_iterative_deepening(state->context, &limits);
        emit_multipv_info(state);
        char buffer[16];
        move_to_uci(&best, buffer, sizeof(buffer));
        if (buffer[0] == '\0') {
            snprintf(buffer, sizeof(buffer), "0000");
        }
        printf("bestmove %s\n", buffer);
        fflush(stdout);
        return;
    }

    data->state = state;
    data->limits = limits;

    state->search_thread_running = 1;
    state->search_thread_active = 1;
    int rc = pthread_create(&state->search_thread, NULL, uci_search_thread, data);
    if (rc != 0) {
        state->search_thread_running = 0;
        state->search_thread_active = 0;
        free(data);
        Move best = search_iterative_deepening(state->context, &limits);
        emit_multipv_info(state);
        char buffer[16];
        move_to_uci(&best, buffer, sizeof(buffer));
        if (buffer[0] == '\0') {
            snprintf(buffer, sizeof(buffer), "0000");
        }
        printf("bestmove %s\n", buffer);
        fflush(stdout);
    }
}

static void handle_bench(UciState* state, char* args) {
    if (!state || !state->context) {
        return;
    }

    stop_search(state);

    SearchLimits limits = { .depth = 12,
                            .movetime_ms = 0,
                            .nodes = 0,
                            .infinite = 0,
                            .multipv = 1,
                            .wtime_ms = 0,
                            .btime_ms = 0,
                            .winc_ms = 0,
                            .binc_ms = 0,
                            .moves_to_go = 0,
                            .ponder = 0 };

    if (args) {
        char* cursor = args;
        char* token = NULL;
        while ((token = next_token(&cursor)) != NULL) {
            if (strcmp(token, "depth") == 0) {
                limits.depth = parse_numeric_token(&cursor);
                if (limits.depth < 0) {
                    limits.depth = 0;
                }
                limits.movetime_ms = 0;
            } else if (strcmp(token, "movetime") == 0) {
                limits.movetime_ms = parse_numeric_token(&cursor);
                if (limits.movetime_ms < 0) {
                    limits.movetime_ms = 0;
                }
                limits.depth = 0;
            }
        }
    }

    bench_run(state->context, &limits);
}

static void uci_loop_impl(UciState* state) {
    char line[1024];
    while (fgets(line, sizeof(line), stdin)) {
        if (strncmp(line, "uci", 3) == 0 && (line[3] == '\0' || isspace((unsigned char)line[3]))) {
            printf("id name SirioC-0.1.0 300925\n");
            printf("id author Jorge Ruiz and Codex Chatgpt creditos\n");
            print_options(state);
            printf("uciok\n");
        } else if (strncmp(line, "isready", 7) == 0 && (line[7] == '\0' || isspace((unsigned char)line[7]))) {
            stop_search(state);
            printf("readyok\n");
        } else if (strncmp(line, "ucinewgame", 10) == 0 && (line[10] == '\0' || isspace((unsigned char)line[10]))) {
            stop_search(state);
            board_set_start_position(state->context->board);
        } else if (strncmp(line, "position", 8) == 0 && (line[8] == '\0' || isspace((unsigned char)line[8]))) {
            stop_search(state);
            handle_position(state, line + 8);
        } else if (strncmp(line, "go", 2) == 0 && (line[2] == '\0' || isspace((unsigned char)line[2]))) {
            handle_go(state, line + 2);
        } else if (strncmp(line, "bench", 5) == 0 &&
                   (line[5] == '\0' || isspace((unsigned char)line[5]))) {
            handle_bench(state, line + 5);
        } else if (strncmp(line, "stop", 4) == 0 && (line[4] == '\0' || isspace((unsigned char)line[4]))) {
            stop_search(state);
        } else if (strncmp(line, "setoption", 9) == 0 && (line[9] == '\0' || isspace((unsigned char)line[9]))) {
            handle_setoption(state, line + 9);
        } else if (strncmp(line, "quit", 4) == 0 && (line[4] == '\0' || isspace((unsigned char)line[4]))) {
            stop_search(state);
            break;
        }
        fflush(stdout);
    }
}

static void uci_state_init(UciState* state, SearchContext* context) {
    memset(state, 0, sizeof(*state));
    state->context = context;
    snprintf(state->eval_file, sizeof(state->eval_file), "%s", UCI_DEFAULT_EVAL_FILE);
    if (eval_has_small_network()) {
        snprintf(state->eval_file_small, sizeof(state->eval_file_small), "%s", UCI_DEFAULT_EVAL_FILE_SMALL);
    } else {
        state->eval_file_small[0] = '\0';
    }
    const char* syzygy_path = tb_get_path();
    if (syzygy_path) {
        snprintf(state->syzygy_path, sizeof(state->syzygy_path), "%s", syzygy_path);
    }
    state->syzygy_probe_depth = tb_get_probe_depth();
    state->syzygy_50_move_rule = tb_get_50_move_rule();
    state->syzygy_probe_limit = tb_get_probe_limit();
    state->move_overhead = context ? context->move_overhead : 10;
    state->hash_size_mb = 64;
    state->threads = 1;
    state->ponder_enabled = 0;
    state->search_thread_running = 0;
    state->search_thread_active = 0;
}

void uci_loop(SearchContext* context) {
    UciState state;
    uci_state_init(&state, context);
    uci_loop_impl(&state);
}

int main(int argc, char* argv[]) {
    bool run_uci = false;
    bool run_bench = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--uci") == 0) {
            run_uci = true;
        } else if (strcmp(argv[i], "--bench") == 0) {
            run_bench = true;
        }
    }

    if (!run_uci && !run_bench) {
        run_uci = true;
    }

    Board board;
    HistoryTable history;
    TranspositionTable tt;
    SearchContext context;

    board_init(&board);
    history_init(&history);
    const int default_hash_mb = 64;
    size_t bytes = (size_t)default_hash_mb * 1024ULL * 1024ULL;
    size_t entry_size = sizeof(TranspositionEntry);
    size_t entries = bytes / entry_size;
    if (entries < 1024) {
        entries = 1024;
    }
    transposition_init(&tt, entries);
    tb_initialize();
    search_init(&context, &board, &tt, &history);
    board_set_start_position(&board);
    eval_init();

    if (run_bench && !run_uci) {
        bench_run(&context, NULL);
    }
    if (run_uci) {
        uci_loop(&context);
    }

    transposition_free(&tt);
    eval_shutdown();
    return 0;
}

