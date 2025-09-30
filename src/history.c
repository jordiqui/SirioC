#include "history.h"

#include "util.h"

void history_init(HistoryTable* history) {
    if (history == NULL) {
        return;
    }
    util_zero_memory(history, sizeof(*history));
}

void history_update(HistoryTable* history, const Move* move, int bonus) {
    if (history == NULL || move == NULL) {
        return;
    }
    enum Color color = (bonus >= 0) ? COLOR_WHITE : COLOR_BLACK;
    int value = history->history[color][move->piece][move->to];
    value += bonus;
    history->history[color][move->piece][move->to] = value;
}

int history_get(const HistoryTable* history, const Move* move) {
    if (history == NULL || move == NULL) {
        return 0;
    }
    enum Color color = COLOR_WHITE;
    return history->history[color][move->piece][move->to];
}

