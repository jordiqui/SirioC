#include "bits.h"

#if defined(__GNUC__)
#include <x86intrin.h>
#endif

int bits_count(Bitboard bb) {
#if defined(__GNUC__)
    return __builtin_popcountll(bb);
#else
    int count = 0;
    while (bb) {
        bb &= bb - 1;
        ++count;
    }
    return count;
#endif
}

int bits_popcount(Bitboard bb) {
    return bits_count(bb);
}

Square bits_ls1b(Bitboard bb) {
    if (bb == 0) {
        return -1;
    }
#if defined(__GNUC__)
    return (Square)__builtin_ctzll(bb);
#else
    Square index = 0;
    while ((bb & 1ULL) == 0ULL) {
        bb >>= 1ULL;
        ++index;
    }
    return index;
#endif
}

Bitboard bits_pop_lsb(Bitboard* bb) {
    if (bb == NULL || *bb == 0ULL) {
        return 0ULL;
    }
    Bitboard lsb = *bb & -(*bb);
    *bb &= *bb - 1ULL;
    return lsb;
}

