#include "pyrrhic/tbprobe.h"

#include <ctype.h>
#include <string.h>

size_t sirio_tb_normalize_fen(const char* fen, char* buffer, size_t capacity) {
    if (!fen || !buffer || capacity == 0) {
        return 0;
    }

    size_t out = 0;
    int previous_space = 1;

    while (*fen && *fen != '\n' && *fen != '\r') {
        char ch = *fen++;
        if (ch == '_') {
            ch = ' ';
        }
        if (ch == '\t') {
            ch = ' ';
        }

        if (isspace((unsigned char)ch)) {
            if (previous_space) {
                continue;
            }
            previous_space = 1;
            ch = ' ';
        } else {
            previous_space = 0;
        }

        if (out + 1 >= capacity) {
            break;
        }

        buffer[out++] = ch;
    }

    if (out > 0 && buffer[out - 1] == ' ') {
        --out;
    }

    buffer[out] = '\0';
    return out;
}

