#include "bench.h"

#include "board.h"
#include "move.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void bench_trim(char* text) {
    if (!text) {
        return;
    }

    char* start = text;
    while (*start && isspace((unsigned char)*start)) {
        ++start;
    }

    char* end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) {
        --end;
    }
    *end = '\0';

    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }
}

void bench_run(SearchContext* context, const SearchLimits* limits) {
    if (context == NULL) {
        return;
    }

    SearchLimits bench_limits = { .depth = 12,
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

    if (limits) {
        bench_limits = *limits;
    }

    const char* paths[] = { "resources/bench.fens", "../resources/bench.fens" };
    FILE* file = NULL;
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        file = fopen(paths[i], "r");
        if (file) {
            break;
        }
    }

    if (!file) {
        printf("info string Failed to open resources/bench.fens\n");
        fflush(stdout);
        return;
    }

    uint64_t total_nodes = 0;
    uint64_t total_time = 0;
    int positions = 0;

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        char* cursor = line;
        while (*cursor && isspace((unsigned char)*cursor)) {
            ++cursor;
        }
        if (*cursor == '\0' || *cursor == '#') {
            continue;
        }

        char* newline = strpbrk(cursor, "\r\n");
        if (newline) {
            *newline = '\0';
        }

        bench_trim(cursor);
        if (*cursor == '\0') {
            continue;
        }

        if (!board_set_fen(context->board, cursor)) {
            printf("info string Invalid FEN in bench file: %s\n", cursor);
            fflush(stdout);
            continue;
        }

        context->stop = 0;
        Move best = search_iterative_deepening(context, &bench_limits);
        total_nodes += context->nodes;
        total_time += context->last_search_time_ms;
        ++positions;

        char move_buffer[16];
        move_to_uci(&best, move_buffer, sizeof(move_buffer));
        if (move_buffer[0] == '\0') {
            snprintf(move_buffer, sizeof(move_buffer), "0000");
        }

        printf("bench position %d bestmove %s nodes %" PRIu64 " time %" PRIu64 "\n",
               positions,
               move_buffer,
               context->nodes,
               context->last_search_time_ms);
        fflush(stdout);
    }

    fclose(file);

    if (positions == 0) {
        printf("bench summary positions 0 time 0 nodes 0 nps 0\n");
        fflush(stdout);
        board_set_start_position(context->board);
        return;
    }

    if (total_time == 0) {
        total_time = 1;
    }

    uint64_t nps = (total_nodes * 1000ULL) / total_time;

    printf("bench summary positions %d time %" PRIu64 " nodes %" PRIu64 " nps %" PRIu64 "\n",
           positions,
           total_time,
           total_nodes,
           nps);
    fflush(stdout);

    board_set_start_position(context->board);
}

