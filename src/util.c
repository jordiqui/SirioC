#include "util.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void util_zero_memory(void* ptr, size_t size) {
    if (ptr == NULL || size == 0) {
        return;
    }
    memset(ptr, 0, size);
}

void util_copy_memory(void* dst, const void* src, size_t size) {
    if (dst == NULL || src == NULL || size == 0) {
        return;
    }
    memcpy(dst, src, size);
}

void util_log(const char* message) {
    if (message == NULL) {
        return;
    }
    fprintf(stderr, "%s\n", message);
}

void util_snprintf(char* buffer, size_t size, const char* fmt, ...) {
    if (buffer == NULL || size == 0 || fmt == NULL) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, size, fmt, args);
    va_end(args);
}

