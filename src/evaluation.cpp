#include "sirio/evaluation.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>

#include "sirio/bitboard.hpp"

namespace sirio {

namespace {

constexpr std::array<int, 6> piece_values = {100, 320, 330, 500, 900, 0};
constexpr int bishop_pair_bonus = 40;
constexpr int endgame_material_threshold = 1300;
constexpr int king_distance_scale = 12;
constexpr int king_corner_scale = 6;
constexpr int king_opposition_bonus = 20;

constexpr std::array<int, 64> pawn_table = {
    0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
    5,  5,  10, 25, 25, 10, 5,  5,
    0,  0,  0,  20, 20, 0,  0,  0,
    5, -5, -10, 0,  0, -10, -5, 5,
    5, 10, 10, -20, -20, 10, 10, 5,
    0,  0,  0,  0,  0,  0,  0,  0};

constexpr std::array<int, 64> knight_table = {
    -50, -40, -30, -30, -30, -30, -40, -50,
    -40, -20, 0,   5,   5,   0,   -20, -40,
    -30, 5,   10,  15,  15,  10,  5,   -30,
    -30, 0,   15,  20,  20,  15,  0,   -30,
    -30, 5,   15,  20,  20,  15,  5,   -30,
    -30, 0,   10,  15,  15,  10,  0,   -30,
    -40, -20, 0,   0,   0,   0,   -20, -40,
    -50, -40, -30, -30, -30, -30, -40, -50};

constexpr std::array<int, 64> bishop_table = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10, 0,   0,   0,   0,   0,   0,   -10,
    -10, 0,   5,   10,  10,  5,   0,   -10,
    -10, 5,   5,   10,  10,  5,   5,   -10,
    -10, 0,   10,  10,  10,  10,  0,   -10,
    -10, 10,  10,  10,  10,  10,  10,  -10,
    -10, 5,   0,   0,   0,   0,   5,   -10,
    -20, -10, -10, -10, -10, -10, -10, -20};

constexpr std::array<int, 64> rook_table = {
    0, 0, 0, 0, 0, 0, 0, 0,
    5, 10, 10, 10, 10, 10, 10, 5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    0, 0, 0, 5, 5, 0, 0, 0};

constexpr std::array<int, 64> queen_table = {
    -20, -10, -10, -5, -5, -10, -10, -20,
    -10, 0,   0,   0,  0,  0,  0,  -10,
    -10, 0,   5,   5,  5,  5,  0,  -10,
    -5,  0,   5,   5,  5,  5,  0,  -5,
    0,   0,   5,   5,  5,  5,  0,  -5,
    -10, 5,   5,   5,  5,  5,  0,  -10,
    -10, 0,   5,   0,  0,  0,  0,  -10,
    -20, -10, -10, -5, -5, -10, -10, -20};

constexpr std::array<int, 64> king_table = {
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -20, -30, -30, -40, -40, -30, -30, -20,
    -10, -20, -20, -20, -20, -20, -20, -10,
    20,  20,  0,   0,   0,   0,   20,  20,
    20,  30,  10,  0,   0,   10,  30,  20};

const std::array<const std::array<int, 64> *, 6> piece_square_tables = {
    &pawn_table, &knight_table, &bishop_table, &rook_table, &queen_table, &king_table};

int mirror_square(int square) { return square ^ 56; }

constexpr std::array<int, 64 * 64> generate_king_distance_table() {
    std::array<int, 64 * 64> table{};
    for (int from = 0; from < 64; ++from) {
        for (int to = 0; to < 64; ++to) {
            int file_diff = (from % 8) - (to % 8);
            if (file_diff < 0) {
                file_diff = -file_diff;
            }
            int rank_diff = (from / 8) - (to / 8);
            if (rank_diff < 0) {
                rank_diff = -rank_diff;
            }
            table[static_cast<std::size_t>(from) * 64 + static_cast<std::size_t>(to)] =
                std::max(file_diff, rank_diff);
        }
    }
    return table;
}

constexpr std::array<int, 64> generate_corner_distance_table() {
    std::array<int, 64> table{};
    constexpr std::array<int, 4> corners = {0, 7, 56, 63};
    for (int square = 0; square < 64; ++square) {
        int best = 8;
        for (int corner : corners) {
            int file_diff = (square % 8) - (corner % 8);
            if (file_diff < 0) {
                file_diff = -file_diff;
            }
            int rank_diff = (square / 8) - (corner / 8);
            if (rank_diff < 0) {
                rank_diff = -rank_diff;
            }
            int distance = std::max(file_diff, rank_diff);
            if (distance < best) {
                best = distance;
            }
        }
        table[static_cast<std::size_t>(square)] = best;
    }
    return table;
}

constexpr auto king_distance_table = generate_king_distance_table();
constexpr auto king_corner_distance_table = generate_corner_distance_table();

}  // namespace

int evaluate(const Board &board) {
    int score = 0;
    int material_white = 0;
    int material_black = 0;
    for (int color_index = 0; color_index < 2; ++color_index) {
        Color color = color_index == 0 ? Color::White : Color::Black;
        for (std::size_t piece_index = 0; piece_index < piece_values.size(); ++piece_index) {
            PieceType type = static_cast<PieceType>(piece_index);
            Bitboard pieces = board.pieces(color, type);
            while (pieces) {
                int square = pop_lsb(pieces);
                int base_value = piece_values[piece_index];
                int table_index = color == Color::White ? square : mirror_square(square);
                int ps_value = (*piece_square_tables[piece_index])[table_index];
                int total = base_value + ps_value;
                score += color == Color::White ? total : -total;
                if (color == Color::White) {
                    material_white += base_value;
                } else {
                    material_black += base_value;
                }
            }
        }
    }

    if (board.has_bishop_pair(Color::White)) {
        score += bishop_pair_bonus;
    }
    if (board.has_bishop_pair(Color::Black)) {
        score -= bishop_pair_bonus;
    }

    int max_material = std::max(material_white, material_black);
    if (max_material <= endgame_material_threshold) {
        int white_king = board.king_square(Color::White);
        int black_king = board.king_square(Color::Black);
        int distance = king_distance_table[static_cast<std::size_t>(white_king) * 64 +
                                           static_cast<std::size_t>(black_king)];
        int closeness = 7 - distance;
        if (closeness < 0) {
            closeness = 0;
        }
        int advantage = material_white - material_black;
        if (advantage > 0) {
            score += closeness * king_distance_scale;
            int corner_distance = king_corner_distance_table[static_cast<std::size_t>(black_king)];
            score += (7 - corner_distance) * king_corner_scale;
            int friendly_corner_distance =
                king_corner_distance_table[static_cast<std::size_t>(white_king)];
            score -= friendly_corner_distance * (king_corner_scale / 2);
            int file_diff = std::abs((white_king % 8) - (black_king % 8));
            int rank_diff = std::abs((white_king / 8) - (black_king / 8));
            if ((file_diff == 0 || rank_diff == 0) && ((distance & 1) == 1)) {
                score += king_opposition_bonus;
            }
        } else if (advantage < 0) {
            score -= closeness * king_distance_scale;
            int corner_distance = king_corner_distance_table[static_cast<std::size_t>(white_king)];
            score -= (7 - corner_distance) * king_corner_scale;
            int friendly_corner_distance =
                king_corner_distance_table[static_cast<std::size_t>(black_king)];
            score += friendly_corner_distance * (king_corner_scale / 2);
            int file_diff = std::abs((white_king % 8) - (black_king % 8));
            int rank_diff = std::abs((white_king / 8) - (black_king / 8));
            if ((file_diff == 0 || rank_diff == 0) && ((distance & 1) == 1)) {
                score -= king_opposition_bonus;
            }
        }
    }

    return score;
}

}  // namespace sirio

