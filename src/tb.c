#include "tb.h"

#include <string.h>

static char g_syzygy_path[1024] = "";
static int g_syzygy_probe_depth = 0;
static int g_syzygy_50_move_rule = 1;
static int g_syzygy_probe_limit = 6;

void tb_init(void) {
}

void tb_set_path(const char* path) {
    if (!path) {
        g_syzygy_path[0] = '\0';
        return;
    }
    size_t len = strlen(path);
    if (len >= sizeof(g_syzygy_path)) {
        len = sizeof(g_syzygy_path) - 1;
    }
    memcpy(g_syzygy_path, path, len);
    g_syzygy_path[len] = '\0';
}

const char* tb_get_path(void) {
    return g_syzygy_path;
}

void tb_set_probe_depth(int depth) {
    if (depth < 0) {
        depth = 0;
    }
    g_syzygy_probe_depth = depth;
}

int tb_get_probe_depth(void) {
    return g_syzygy_probe_depth;
}

void tb_set_50_move_rule(int enabled) {
    g_syzygy_50_move_rule = enabled ? 1 : 0;
}

int tb_get_50_move_rule(void) {
    return g_syzygy_50_move_rule;
}

void tb_set_probe_limit(int limit) {
    if (limit < 0) {
        limit = 0;
    }
    g_syzygy_probe_limit = limit;
}

int tb_get_probe_limit(void) {
    return g_syzygy_probe_limit;
}

Value tb_probe(const Board* board) {
    (void)board;
    return VALUE_NONE;
}

