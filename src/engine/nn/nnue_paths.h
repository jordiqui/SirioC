#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Attempts to locate the given NNUE file name across common resource
// directories. On success, the resolved absolute path is copied into
// `out`, including the null terminator, and the function returns 1.
// On failure the buffer is untouched and 0 is returned.
int sirio_nnue_locate(const char* file_name, char* out, size_t out_size);

#ifdef __cplusplus
}
#endif

