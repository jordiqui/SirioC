#include "sirio/evaluation.hpp"

#include <array>

#include "sirio/bitboard.hpp"

namespace sirio {

namespace {

constexpr std::array<int, 6> piece_values = {100, 320, 330, 500, 900, 0};

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

}  // namespace

int evaluate(const Board &board) {
    int score = 0;
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
            }
        }
    }

    return score;
}

}  // namespace sirio

