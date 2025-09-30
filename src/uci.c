#include "uci.h"

#include <stdio.h>
#include <string.h>

#include "board.h"
#include "history.h"
#include "movegen.h"
#include "search.h"
#include "transposition.h"
#include "eval.h"

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

int main(void) {
    Board board;
    HistoryTable history;
    TranspositionTable tt;
    SearchContext context;

    board_init(&board);
    eval_init();
    if (!eval_load_network("resources/network.dat")) {
        fprintf(stderr, "warning: could not load network weights from resources/network.dat, using defaults\n");
    }
    history_init(&history);
    transposition_init(&tt, 1 << 16);
    search_init(&context, &board, &tt, &history);
    board_set_start_position(&board);

    uci_loop(&context);

    transposition_free(&tt);
    return 0;
}

