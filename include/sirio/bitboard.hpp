#pragma once

#include <cstdint>

namespace sirio {

using Bitboard = std::uint64_t;

constexpr Bitboard one_bit(int square) {
    return Bitboard{1} << square;
}

constexpr int rank_of(int square) {
    return square / 8;
}

constexpr int file_of(int square) {
    return square % 8;
}

constexpr Bitboard file_a_mask = 0x0101010101010101ULL;
constexpr Bitboard file_b_mask = 0x0202020202020202ULL;
constexpr Bitboard file_g_mask = 0x4040404040404040ULL;
constexpr Bitboard file_h_mask = 0x8080808080808080ULL;

constexpr Bitboard not_file_a_mask = ~file_a_mask;
constexpr Bitboard not_file_h_mask = ~file_h_mask;
constexpr Bitboard not_file_ab_mask = ~(file_a_mask | file_b_mask);
constexpr Bitboard not_file_gh_mask = ~(file_g_mask | file_h_mask);

inline Bitboard pawn_attacks_white(Bitboard pawns) {
    return ((pawns & not_file_a_mask) << 7) | ((pawns & not_file_h_mask) << 9);
}

inline Bitboard pawn_attacks_black(Bitboard pawns) {
    return ((pawns & not_file_h_mask) >> 7) | ((pawns & not_file_a_mask) >> 9);
}

inline Bitboard knight_attacks(int square) {
    const Bitboard knights = one_bit(square);
    Bitboard attacks = 0;
    attacks |= (knights & not_file_h_mask) << 17;
    attacks |= (knights & not_file_a_mask) << 15;
    attacks |= (knights & not_file_ab_mask) << 6;
    attacks |= (knights & not_file_gh_mask) << 10;
    attacks |= (knights & not_file_a_mask) >> 17;
    attacks |= (knights & not_file_h_mask) >> 15;
    attacks |= (knights & not_file_gh_mask) >> 6;
    attacks |= (knights & not_file_ab_mask) >> 10;
    return attacks;
}

inline Bitboard king_attacks(int square) {
    const Bitboard king = one_bit(square);
    Bitboard attacks = 0;
    attacks |= (king & not_file_h_mask) << 1;
    attacks |= king << 8;
    attacks |= (king & not_file_a_mask) << 7;
    attacks |= (king & not_file_h_mask) << 9;
    attacks |= (king & not_file_a_mask) >> 1;
    attacks |= king >> 8;
    attacks |= (king & not_file_h_mask) >> 7;
    attacks |= (king & not_file_a_mask) >> 9;
    return attacks;
}

inline Bitboard ray_attacks(int square, int file_step, int rank_step, Bitboard occupancy) {
    Bitboard attacks = 0;
    int file = file_of(square);
    int rank = rank_of(square);
    int f = file + file_step;
    int r = rank + rank_step;
    while (f >= 0 && f < 8 && r >= 0 && r < 8) {
        const int index = r * 8 + f;
        const Bitboard bit = one_bit(index);
        attacks |= bit;
        if (occupancy & bit) {
            break;
        }
        f += file_step;
        r += rank_step;
    }
    return attacks;
}

inline Bitboard bishop_attacks(int square, Bitboard occupancy) {
    return ray_attacks(square, 1, 1, occupancy) |
           ray_attacks(square, -1, 1, occupancy) |
           ray_attacks(square, 1, -1, occupancy) |
           ray_attacks(square, -1, -1, occupancy);
}

inline Bitboard rook_attacks(int square, Bitboard occupancy) {
    return ray_attacks(square, 1, 0, occupancy) |
           ray_attacks(square, -1, 0, occupancy) |
           ray_attacks(square, 0, 1, occupancy) |
           ray_attacks(square, 0, -1, occupancy);
}

inline Bitboard queen_attacks(int square, Bitboard occupancy) {
    return bishop_attacks(square, occupancy) | rook_attacks(square, occupancy);
}

}  // namespace sirio

