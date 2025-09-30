#include "uci.h"

#include <ctype.h>
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

#define UCI_DEFAULT_EVAL_FILE "resources/sirio_default.nnue"
#define UCI_DEFAULT_EVAL_FILE_SMALL "resources/sirio_small.nnue"

typedef struct {
    SearchContext* context;
    char eval_file[512];
    char eval_file_small[512];
    char syzygy_path[1024];
    int syzygy_probe_depth;
    int syzygy_50_move_rule;
    int syzygy_probe_limit;
    int move_overhead;
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
    printf("option name EvalFileSmall type string default %s\n", state->eval_file_small[0] ? state->eval_file_small : "");
    printf("option name SyzygyPath type string default %s\n", state->syzygy_path);
    printf("option name SyzygyProbeDepth type spin default %d min 0 max 64\n", state->syzygy_probe_depth);
    printf("option name Syzygy50MoveRule type check default %s\n", state->syzygy_50_move_rule ? "true" : "false");
    printf("option name SyzygyProbeLimit type spin default %d min 0 max 7\n", state->syzygy_probe_limit);
    printf("option name Move Overhead type spin default %d min 0 max 5000\n", state->move_overhead);
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
            fprintf(stderr, "warning: failed to load NNUE file %s\n", option_value);
        }
    } else if (strcmp(name_ptr, "EvalFileSmall") == 0) {
        if (option_value && *option_value) {
            if (eval_load_small_network(option_value)) {
                snprintf(state->eval_file_small, sizeof(state->eval_file_small), "%s", option_value);
            } else {
                fprintf(stderr, "warning: failed to load secondary NNUE file %s\n", option_value);
            }
        } else {
            eval_load_small_network(NULL);
            state->eval_file_small[0] = '\0';
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
    SearchLimits limits = { .depth = 1, .movetime_ms = 0, .nodes = 0, .infinite = 0, .multipv = 1 };
    if (args) {
        char* depth_ptr = strstr(args, "depth");
        if (depth_ptr) {
            depth_ptr += 5;
            limits.depth = atoi(depth_ptr);
            if (limits.depth <= 0) {
                limits.depth = 1;
            }
        }
        char* movetime_ptr = strstr(args, "movetime");
        if (movetime_ptr) {
            movetime_ptr += 8;
            while (*movetime_ptr == ' ') {
                ++movetime_ptr;
            }
            limits.movetime_ms = atoi(movetime_ptr);
            if (limits.movetime_ms < 0) {
                limits.movetime_ms = 0;
            }
            int overhead = state->context ? state->context->move_overhead : state->move_overhead;
            if (limits.movetime_ms > overhead) {
                limits.movetime_ms -= overhead;
            } else {
                limits.movetime_ms = 0;
            }
        }
    }

    Move best = search_iterative_deepening(state->context, &limits);
    for (size_t index = 0; index < state->context->pv_count; ++index) {
        char pv_buffer[16];
        move_to_uci(&state->context->pv_moves[index], pv_buffer, sizeof(pv_buffer));
        printf("info multipv %zu score cp %d pv %s\n", index + 1,
               state->context->pv_values[index], pv_buffer);
    }
    char buffer[16];
    move_to_uci(&best, buffer, sizeof(buffer));
    if (buffer[0] == '\0') {
        snprintf(buffer, sizeof(buffer), "0000");
    }
    printf("bestmove %s\n", buffer);
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
            printf("readyok\n");
        } else if (strncmp(line, "ucinewgame", 10) == 0 && (line[10] == '\0' || isspace((unsigned char)line[10]))) {
            board_set_start_position(state->context->board);
        } else if (strncmp(line, "position", 8) == 0 && (line[8] == '\0' || isspace((unsigned char)line[8]))) {
            handle_position(state, line + 8);
        } else if (strncmp(line, "go", 2) == 0 && (line[2] == '\0' || isspace((unsigned char)line[2]))) {
            handle_go(state, line + 2);
        } else if (strncmp(line, "setoption", 9) == 0 && (line[9] == '\0' || isspace((unsigned char)line[9]))) {
            handle_setoption(state, line + 9);
        } else if (strncmp(line, "quit", 4) == 0 && (line[4] == '\0' || isspace((unsigned char)line[4]))) {
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
    transposition_init(&tt, 1 << 16);
    search_init(&context, &board, &tt, &history);
    board_set_start_position(&board);
    eval_init();

    if (run_bench && !run_uci) {
        bench_run(&context);
    }
    if (run_uci) {
        uci_loop(&context);
    }

    transposition_free(&tt);
    eval_shutdown();
    return 0;
}

