#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void util_zero_memory(void* ptr, size_t size);
void util_copy_memory(void* dst, const void* src, size_t size);
void util_log(const char* message);
void util_snprintf(char* buffer, size_t size, const char* fmt, ...);

#ifdef __cplusplus
} /* extern "C" */
#endif

