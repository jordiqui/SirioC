#pragma once

#include <stddef.h>

#include "pyrrhic/stdendian.h"
#include "pyrrhic/tbconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int wdl;
    int dtm;
} sirio_tb_result;

int sirio_tb_init(const char* csv_path);
int sirio_tb_is_ready(void);
int sirio_tb_probe_fen(const char* fen, sirio_tb_result* result);
void sirio_tb_shutdown(void);
size_t sirio_tb_normalize_fen(const char* fen, char* buffer, size_t capacity);

#ifdef __cplusplus
}
#endif

