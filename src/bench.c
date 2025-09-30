#include "bench.h"

#include <stdio.h>

void bench_run(SearchContext* context) {
    if (context == NULL) {
        return;
    }

    SearchLimits limits = { .depth = 4,
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
    Move move = search_iterative_deepening(context, &limits);
    printf("bench bestmove %d%d value %d\n", move.from, move.to, context->best_value);
}

