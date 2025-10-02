#include "attacks.h"

#include <stddef.h>

static Bitboard g_knight_attacks[64];
static Bitboard g_king_attacks[64];
static Bitboard g_pawn_attacks[COLOR_NB][64];

static Bitboard mask_knight(Square square) {
    const int file = square % 8;
    const int rank = square / 8;
    Bitboard attacks = 0ULL;

    const int offsets[8][2] = {
        {1, 2}, {2, 1}, {-1, 2}, {-2, 1},
        {1, -2}, {2, -1}, {-1, -2}, {-2, -1}
    };

    for (size_t i = 0; i < 8; ++i) {
        int nf = file + offsets[i][0];
        int nr = rank + offsets[i][1];
        if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) {
            attacks |= 1ULL << (nr * 8 + nf);
        }
    }
    return attacks;
}

static Bitboard mask_king(Square square) {
    const int file = square % 8;
    const int rank = square / 8;
    Bitboard attacks = 0ULL;

    for (int df = -1; df <= 1; ++df) {
        for (int dr = -1; dr <= 1; ++dr) {
            if (df == 0 && dr == 0) {
                continue;
            }
            int nf = file + df;
            int nr = rank + dr;
            if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) {
                attacks |= 1ULL << (nr * 8 + nf);
            }
        }
    }

    return attacks;
}

static Bitboard mask_pawn(Square square, enum Color color) {
    const int file = square % 8;
    const int rank = square / 8;
    Bitboard attacks = 0ULL;

    int forward = (color == COLOR_WHITE) ? 1 : -1;

    int nf = file - 1;
    int nr = rank + forward;
    if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) {
        attacks |= 1ULL << (nr * 8 + nf);
    }

    nf = file + 1;
    nr = rank + forward;
    if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) {
        attacks |= 1ULL << (nr * 8 + nf);
    }

    return attacks;
}

void attacks_init(void) {
    for (Square sq = 0; sq < 64; ++sq) {
        g_knight_attacks[sq] = mask_knight(sq);
        g_king_attacks[sq] = mask_king(sq);
        g_pawn_attacks[COLOR_WHITE][sq] = mask_pawn(sq, COLOR_WHITE);
        g_pawn_attacks[COLOR_BLACK][sq] = mask_pawn(sq, COLOR_BLACK);
    }
}

Bitboard attacks_knight(Square square) {
    if (square < 0 || square >= 64) {
        return 0ULL;
    }
    return g_knight_attacks[square];
}

Bitboard attacks_king(Square square) {
    if (square < 0 || square >= 64) {
        return 0ULL;
    }
    return g_king_attacks[square];
}

Bitboard attacks_pawn(Square square, enum Color color) {
    if (square < 0 || square >= 64 || color < 0 || color >= COLOR_NB) {
        return 0ULL;
    }
    return g_pawn_attacks[color][square];
}

Bitboard attacks_bishop(Square square, Bitboard occupancy) {
    (void)occupancy;
    if (square < 0 || square >= 64) {
        return 0ULL;
    }

    Bitboard attacks = 0ULL;
    int file = square % 8;
    int rank = square / 8;

    for (int df = -1; df <= 1; df += 2) {
        for (int dr = -1; dr <= 1; dr += 2) {
            int nf = file + df;
            int nr = rank + dr;
            while (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) {
                attacks |= 1ULL << (nr * 8 + nf);
                nf += df;
                nr += dr;
            }
        }
    }

    return attacks;
}

Bitboard attacks_rook(Square square, Bitboard occupancy) {
    (void)occupancy;
    if (square < 0 || square >= 64) {
        return 0ULL;
    }

    Bitboard attacks = 0ULL;
    int file = square % 8;
    int rank = square / 8;

    for (int df = -1; df <= 1; ++df) {
        if (df == 0) {
            continue;
        }
        int nf = file + df;
        while (nf >= 0 && nf < 8) {
            attacks |= 1ULL << (rank * 8 + nf);
            nf += df;
        }
    }

    for (int dr = -1; dr <= 1; ++dr) {
        if (dr == 0) {
            continue;
        }
        int nr = rank + dr;
        while (nr >= 0 && nr < 8) {
            attacks |= 1ULL << (nr * 8 + file);
            nr += dr;
        }
    }

    return attacks;
}

Bitboard attacks_queen(Square square, Bitboard occupancy) {
    return attacks_bishop(square, occupancy) | attacks_rook(square, occupancy);
}

