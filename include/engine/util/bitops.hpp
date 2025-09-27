#pragma once
#include <cstdint>

namespace engine::bitops {
    inline int popcount64(uint64_t x) {
    #if defined(__GNUG__) || defined(__clang__)
        return __builtin_popcountll(x);
    #else
        int c = 0; while (x) { x &= (x - 1); ++c; } return c;
    #endif
    }
}
