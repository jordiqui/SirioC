#include "movepick.h"

static int move_value(const Move* move) {
    if (move == NULL) {
        return 0;
    }
    return (move->capture != PIECE_NONE) ? 1 : 0;
}

void movepick_sort(MoveList* list) {
    if (list == NULL) {
        return;
    }

    for (size_t i = 0; i < list->size; ++i) {
        size_t best = i;
        for (size_t j = i + 1; j < list->size; ++j) {
            if (move_value(&list->moves[j]) > move_value(&list->moves[best])) {
                best = j;
            }
        }
        if (best != i) {
            Move tmp = list->moves[i];
            list->moves[i] = list->moves[best];
            list->moves[best] = tmp;
        }
    }
}

