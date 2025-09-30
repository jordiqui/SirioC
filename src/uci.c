#include "uci.h"

#include <stdio.h>
#include <string.h>

#include "board.h"
#include "movegen.h"

void uci_loop(SearchContext* context) {
    char line[1024];
    while (fgets(line, sizeof(line), stdin)) {
        if (strncmp(line, "uci", 3) == 0) {
            printf("id name SirioC\n");
            printf("id author OpenAI\n");
            printf("uciok\n");
        } else if (strncmp(line, "isready", 7) == 0) {
            printf("readyok\n");
        } else if (strncmp(line, "position startpos", 18) == 0) {
            board_set_start_position(context->board);
        } else if (strncmp(line, "go", 2) == 0) {
            SearchLimits limits = { .depth = 1, .movetime_ms = 0, .nodes = 0, .infinite = 0 };
            Move best = search_iterative_deepening(context, &limits);
            printf("bestmove %d%d\n", best.from, best.to);
        } else if (strncmp(line, "quit", 4) == 0) {
            break;
        }
        fflush(stdout);
    }
}

