#include "sirio/bitboard.hpp"

#include <array>
#include <mutex>

namespace sirio {

namespace {

constexpr int kMaxBishopRelevantBits = 9;
constexpr int kMaxRookRelevantBits = 12;

using BishopAttackTable = std::array<std::array<Bitboard, 1 << kMaxBishopRelevantBits>, 64>;
using RookAttackTable = std::array<std::array<Bitboard, 1 << kMaxRookRelevantBits>, 64>;

std::once_flag sliding_table_init_flag;

std::array<Bitboard, 64> bishop_masks{};
std::array<Bitboard, 64> rook_masks{};

std::array<std::array<int, kMaxBishopRelevantBits>, 64> bishop_relevant_squares{};
std::array<std::array<int, kMaxRookRelevantBits>, 64> rook_relevant_squares{};

std::array<int, 64> bishop_relevant_count{};
std::array<int, 64> rook_relevant_count{};

BishopAttackTable bishop_attacks_table{};
RookAttackTable rook_attacks_table{};

Bitboard bishop_attacks_on_the_fly(int square, Bitboard occupancy) {
    return ray_attacks(square, 1, 1, occupancy) | ray_attacks(square, -1, 1, occupancy) |
           ray_attacks(square, 1, -1, occupancy) | ray_attacks(square, -1, -1, occupancy);
}

Bitboard rook_attacks_on_the_fly(int square, Bitboard occupancy) {
    return ray_attacks(square, 1, 0, occupancy) | ray_attacks(square, -1, 0, occupancy) |
           ray_attacks(square, 0, 1, occupancy) | ray_attacks(square, 0, -1, occupancy);
}

Bitboard bishop_mask(int square) {
    Bitboard mask = 0;
    int file = file_of(square);
    int rank = rank_of(square);

    for (int f = file + 1, r = rank + 1; f < 7 && r < 7; ++f, ++r) {
        mask |= one_bit(r * 8 + f);
    }
    for (int f = file - 1, r = rank + 1; f > 0 && r < 7; --f, ++r) {
        mask |= one_bit(r * 8 + f);
    }
    for (int f = file + 1, r = rank - 1; f < 7 && r > 0; ++f, --r) {
        mask |= one_bit(r * 8 + f);
    }
    for (int f = file - 1, r = rank - 1; f > 0 && r > 0; --f, --r) {
        mask |= one_bit(r * 8 + f);
    }
    return mask;
}

Bitboard rook_mask(int square) {
    Bitboard mask = 0;
    int file = file_of(square);
    int rank = rank_of(square);

    for (int f = file + 1; f < 7; ++f) {
        mask |= one_bit(rank * 8 + f);
    }
    for (int f = file - 1; f > 0; --f) {
        mask |= one_bit(rank * 8 + f);
    }
    for (int r = rank + 1; r < 7; ++r) {
        mask |= one_bit(r * 8 + file);
    }
    for (int r = rank - 1; r > 0; --r) {
        mask |= one_bit(r * 8 + file);
    }
    return mask;
}

Bitboard subset_to_bitboard(int subset_index, const std::array<int, kMaxBishopRelevantBits> &squares,
                            int count) {
    Bitboard occupancy = 0;
    for (int i = 0; i < count; ++i) {
        if (subset_index & (1 << i)) {
            occupancy |= one_bit(squares[static_cast<std::size_t>(i)]);
        }
    }
    return occupancy;
}

Bitboard subset_to_bitboard(int subset_index, const std::array<int, kMaxRookRelevantBits> &squares,
                            int count) {
    Bitboard occupancy = 0;
    for (int i = 0; i < count; ++i) {
        if (subset_index & (1 << i)) {
            occupancy |= one_bit(squares[static_cast<std::size_t>(i)]);
        }
    }
    return occupancy;
}

std::uint32_t occupancy_to_index(Bitboard occupancy, const std::array<int, kMaxBishopRelevantBits> &squares,
                                 int count) {
    std::uint32_t index = 0;
    for (int i = 0; i < count; ++i) {
        int square = squares[static_cast<std::size_t>(i)];
        if (occupancy & one_bit(square)) {
            index |= static_cast<std::uint32_t>(1u << i);
        }
    }
    return index;
}

std::uint32_t occupancy_to_index(Bitboard occupancy, const std::array<int, kMaxRookRelevantBits> &squares,
                                 int count) {
    std::uint32_t index = 0;
    for (int i = 0; i < count; ++i) {
        int square = squares[static_cast<std::size_t>(i)];
        if (occupancy & one_bit(square)) {
            index |= static_cast<std::uint32_t>(1u << i);
        }
    }
    return index;
}

void initialize_tables() {
    for (int square = 0; square < 64; ++square) {
        Bitboard mask = bishop_mask(square);
        bishop_masks[static_cast<std::size_t>(square)] = mask;
        Bitboard bits = mask;
        int count = 0;
        while (bits) {
            int idx = pop_lsb(bits);
            bishop_relevant_squares[static_cast<std::size_t>(square)][count++] = idx;
        }
        bishop_relevant_count[static_cast<std::size_t>(square)] = count;
        int subset_count = 1 << count;
        for (int index = 0; index < subset_count; ++index) {
            Bitboard occ = subset_to_bitboard(index, bishop_relevant_squares[static_cast<std::size_t>(square)], count);
            bishop_attacks_table[static_cast<std::size_t>(square)][index] =
                bishop_attacks_on_the_fly(square, occ);
        }

        mask = rook_mask(square);
        rook_masks[static_cast<std::size_t>(square)] = mask;
        bits = mask;
        count = 0;
        while (bits) {
            int idx = pop_lsb(bits);
            rook_relevant_squares[static_cast<std::size_t>(square)][count++] = idx;
        }
        rook_relevant_count[static_cast<std::size_t>(square)] = count;
        subset_count = 1 << count;
        for (int index = 0; index < subset_count; ++index) {
            Bitboard occ = subset_to_bitboard(index, rook_relevant_squares[static_cast<std::size_t>(square)], count);
            rook_attacks_table[static_cast<std::size_t>(square)][index] = rook_attacks_on_the_fly(square, occ);
        }
    }
}

void ensure_tables() {
    std::call_once(sliding_table_init_flag, initialize_tables);
}

}  // namespace

void initialize_sliding_attack_tables() {
    ensure_tables();
}

Bitboard bishop_attacks(int square, Bitboard occupancy) {
    ensure_tables();
    Bitboard occ = occupancy & bishop_masks[static_cast<std::size_t>(square)];
    int count = bishop_relevant_count[static_cast<std::size_t>(square)];
    std::uint32_t index = occupancy_to_index(occ, bishop_relevant_squares[static_cast<std::size_t>(square)], count);
    return bishop_attacks_table[static_cast<std::size_t>(square)][index];
}

Bitboard rook_attacks(int square, Bitboard occupancy) {
    ensure_tables();
    Bitboard occ = occupancy & rook_masks[static_cast<std::size_t>(square)];
    int count = rook_relevant_count[static_cast<std::size_t>(square)];
    std::uint32_t index = occupancy_to_index(occ, rook_relevant_squares[static_cast<std::size_t>(square)], count);
    return rook_attacks_table[static_cast<std::size_t>(square)][index];
}

}  // namespace sirio

