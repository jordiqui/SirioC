#include "pyrrhic/tbprobe.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char* key;
    int wdl;
    int dtm;
} sirio_tb_entry;

static sirio_tb_entry* g_entries = NULL;
static size_t g_entry_count = 0;
static size_t g_entry_capacity = 0;
static int g_ready = 0;

static void sirio_tb_free_entries(void) {
    if (!g_entries) {
        return;
    }

    for (size_t index = 0; index < g_entry_count; ++index) {
        free(g_entries[index].key);
    }

    free(g_entries);
    g_entries = NULL;
    g_entry_count = 0;
    g_entry_capacity = 0;
}

static char* sirio_tb_strdup(const char* text) {
    if (!text) {
        return NULL;
    }

    size_t length = strlen(text);
    char* copy = (char*)malloc(length + 1);
    if (!copy) {
        return NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}

static int sirio_tb_ensure_capacity(size_t size) {
    if (g_entry_capacity >= size) {
        return 1;
    }

    size_t new_capacity = g_entry_capacity == 0 ? 16 : g_entry_capacity * 2;
    while (new_capacity < size) {
        new_capacity *= 2;
    }

    sirio_tb_entry* resized = (sirio_tb_entry*)realloc(g_entries, new_capacity * sizeof(sirio_tb_entry));
    if (!resized) {
        return 0;
    }

    g_entries = resized;
    g_entry_capacity = new_capacity;
    return 1;
}

static int sirio_tb_parse_line(const char* line, char** fen_out, int* wdl_out, int* dtm_out) {
    if (!line || !fen_out || !wdl_out || !dtm_out) {
        return 0;
    }

    char buffer[SIRIO_TB_BUFFER_SIZE];
    size_t length = strlen(line);
    if (length >= sizeof(buffer)) {
        length = sizeof(buffer) - 1;
    }
    memcpy(buffer, line, length);
    buffer[length] = '\0';

    if (buffer[0] == '#' || buffer[0] == '\0') {
        return 0;
    }

    char* fen = strtok(buffer, ",");
    char* wdl = strtok(NULL, ",");
    char* dtm = strtok(NULL, ",");

    if (!fen || !wdl) {
        return 0;
    }

    char normalized[SIRIO_TB_BUFFER_SIZE];
    sirio_tb_normalize_fen(fen, normalized, sizeof(normalized));

    *fen_out = sirio_tb_strdup(normalized);
    if (!*fen_out) {
        return 0;
    }

    *wdl_out = atoi(wdl);
    *dtm_out = dtm ? atoi(dtm) : 0;
    return 1;
}

int sirio_tb_init(const char* csv_path) {
    sirio_tb_shutdown();

    const char* path = csv_path ? csv_path : SIRIO_TB_DEFAULT_PATH;
    FILE* file = fopen(path, "r");
    if (!file) {
        g_ready = 0;
        return 0;
    }

    char line[SIRIO_TB_BUFFER_SIZE];
    int line_number = 0;
    while (fgets(line, sizeof(line), file)) {
        ++line_number;
        if (line_number == 1 && strncmp(line, "fen", 3) == 0) {
            continue;
        }

        char* fen = NULL;
        int wdl = 0;
        int dtm = 0;

        if (!sirio_tb_parse_line(line, &fen, &wdl, &dtm)) {
            continue;
        }

        if (!sirio_tb_ensure_capacity(g_entry_count + 1)) {
            free(fen);
            break;
        }

        g_entries[g_entry_count].key = fen;
        g_entries[g_entry_count].wdl = wdl;
        g_entries[g_entry_count].dtm = dtm;
        ++g_entry_count;
    }

    fclose(file);
    g_ready = g_entry_count > 0;
    return g_ready;
}

int sirio_tb_is_ready(void) {
    return g_ready;
}

int sirio_tb_probe_fen(const char* fen, sirio_tb_result* result) {
    if (!fen || !result || !g_ready) {
        return 0;
    }

    char normalized[SIRIO_TB_BUFFER_SIZE];
    sirio_tb_normalize_fen(fen, normalized, sizeof(normalized));

    for (size_t index = 0; index < g_entry_count; ++index) {
        if (strcmp(g_entries[index].key, normalized) == 0) {
            result->wdl = g_entries[index].wdl;
            result->dtm = g_entries[index].dtm;
            return 1;
        }
    }

    return 0;
}

void sirio_tb_shutdown(void) {
    sirio_tb_free_entries();
    g_ready = 0;
}

