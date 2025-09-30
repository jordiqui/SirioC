#include "bench.h"

#include <stdio.h>

void bench_run(SearchContext* context) {
    if (context == NULL) {
        return;
    }

    SearchLimits limits = { .depth = 1, .movetime_ms = 0, .nodes = 0, .infinite = 0 };
    Move move = search_iterative_deepening(context, &limits);
    printf("bench bestmove %d%d value %d\n", move.from, move.to, context->best_value);
}

