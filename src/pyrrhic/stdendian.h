#pragma once

#include <stdint.h>

static inline int sirio_tb_is_little_endian(void) {
    const uint16_t value = 0x1;
    const unsigned char* bytes = (const unsigned char*)&value;
    return bytes[0] == 0x1;
}

