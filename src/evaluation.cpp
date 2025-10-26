#include "sirio/evaluation.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <optional>

#include "sirio/bitboard.hpp"
#include "sirio/endgame.hpp"

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

constexpr Bitboard light_square_mask = 0x55AA55AA55AA55AAULL;
constexpr Bitboard dark_square_mask = 0xAA55AA55AA55AA55ULL;

int mirror_square(int square) { return square ^ 56; }

std::array<int, 8> pawn_file_counts(const Board &board, Color color) {
    std::array<int, 8> counts{};
    Bitboard pawns = board.pieces(color, PieceType::Pawn);
    while (pawns) {
        int sq = pop_lsb(pawns);
        ++counts[file_of(sq)];
    }
    return counts;
}

int evaluate_pawn_structure(const Board &board, Color color,
                            const std::array<int, 8> &friendly_counts,
                            const std::array<int, 8> &enemy_counts) {
    int score = 0;
    for (int file = 0; file < 8; ++file) {
        if (friendly_counts[file] > 1) {
            score -= 12 * (friendly_counts[file] - 1);
        }
    }

    Bitboard pawns = board.pieces(color, PieceType::Pawn);
    Bitboard enemy_pawns = board.pieces(opposite(color), PieceType::Pawn);
    while (pawns) {
        int sq = pop_lsb(pawns);
        int file = file_of(sq);
        int rank = rank_of(sq);

        bool isolated = true;
        if (file > 0 && friendly_counts[file - 1] > 0) {
            isolated = false;
        }
        if (file < 7 && friendly_counts[file + 1] > 0) {
            isolated = false;
        }
        if (isolated) {
            score -= 15;
        }

        bool passed = true;
        for (int adj = std::max(0, file - 1); adj <= std::min(7, file + 1); ++adj) {
            if (enemy_counts[adj] == 0) {
                continue;
            }
            Bitboard mask = 0;
            if (color == Color::White) {
                for (int r = rank + 1; r < 8; ++r) {
                    mask |= one_bit(r * 8 + adj);
                }
            } else {
                for (int r = rank - 1; r >= 0; --r) {
                    mask |= one_bit(r * 8 + adj);
                }
            }
            if (enemy_pawns & mask) {
                passed = false;
                break;
            }
        }
        if (passed) {
            int advancement = color == Color::White ? rank : (7 - rank);
            score += 30 + advancement * 10;
        }
    }

    return color == Color::White ? score : -score;
}

int evaluate_mobility(const Board &board, Color color) {
    Bitboard occupancy_all = board.occupancy();
    Bitboard occupancy_us = board.occupancy(color);
    int score = 0;

    Bitboard knights = board.pieces(color, PieceType::Knight);
    while (knights) {
        int sq = pop_lsb(knights);
        score += std::popcount(knight_attacks(sq) & ~occupancy_us) * 4;
    }

    Bitboard bishops = board.pieces(color, PieceType::Bishop);
    while (bishops) {
        int sq = pop_lsb(bishops);
        score += std::popcount(bishop_attacks(sq, occupancy_all) & ~occupancy_us) * 5;
    }

    Bitboard rooks = board.pieces(color, PieceType::Rook);
    while (rooks) {
        int sq = pop_lsb(rooks);
        score += std::popcount(rook_attacks(sq, occupancy_all) & ~occupancy_us) * 3;
    }

    Bitboard queens = board.pieces(color, PieceType::Queen);
    while (queens) {
        int sq = pop_lsb(queens);
        score += std::popcount(queen_attacks(sq, occupancy_all) & ~occupancy_us) * 2;
    }

    return color == Color::White ? score : -score;
}

int evaluate_king_safety(const Board &board, Color color,
                         const std::array<int, 8> &friendly_counts) {
    int score = 0;
    int king_sq = board.king_square(color);
    if (king_sq < 0) {
        return 0;
    }
    int king_file = file_of(king_sq);
    int king_rank = rank_of(king_sq);

    // Pawn shield
    int shield = 0;
    for (int file_offset = -1; file_offset <= 1; ++file_offset) {
        int file = king_file + file_offset;
        if (file < 0 || file > 7) {
            continue;
        }
        int forward_rank = color == Color::White ? king_rank + 1 : king_rank - 1;
        if (forward_rank < 0 || forward_rank >= 8) {
            continue;
        }
        int square = forward_rank * 8 + file;
        auto piece = board.piece_at(square);
        if (piece && piece->first == color && piece->second == PieceType::Pawn) {
            shield += 15;
        }
    }
    score += shield;

    if (friendly_counts[king_file] == 0) {
        score -= 20;
    }

    Bitboard king_zone = king_attacks(king_sq) | one_bit(king_sq);
    Color enemy = opposite(color);
    Bitboard occupancy = board.occupancy();

    Bitboard enemy_knights = board.pieces(enemy, PieceType::Knight);
    int attack_penalty = 0;
    while (enemy_knights) {
        int sq = pop_lsb(enemy_knights);
        attack_penalty += std::popcount(knight_attacks(sq) & king_zone) * 6;
    }

    Bitboard enemy_bishops = board.pieces(enemy, PieceType::Bishop) | board.pieces(enemy, PieceType::Queen);
    Bitboard tmp = enemy_bishops;
    while (tmp) {
        int sq = pop_lsb(tmp);
        attack_penalty += std::popcount(bishop_attacks(sq, occupancy) & king_zone) * 5;
    }

    Bitboard enemy_rooks = board.pieces(enemy, PieceType::Rook) | board.pieces(enemy, PieceType::Queen);
    tmp = enemy_rooks;
    while (tmp) {
        int sq = pop_lsb(tmp);
        attack_penalty += std::popcount(rook_attacks(sq, occupancy) & king_zone) * 4;
    }

    Bitboard enemy_pawns = board.pieces(enemy, PieceType::Pawn);
    Bitboard pawn_attacks = enemy == Color::White ? pawn_attacks_white(enemy_pawns)
                                                 : pawn_attacks_black(enemy_pawns);
    attack_penalty += std::popcount(pawn_attacks & king_zone) * 7;

    score -= attack_penalty;
    return color == Color::White ? score : -score;
}

int evaluate_minor_pieces(const Board &board, Color color) {
    int score = 0;
    Bitboard friendly_pawns = board.pieces(color, PieceType::Pawn);
    Bitboard enemy_pawns = board.pieces(opposite(color), PieceType::Pawn);
    Bitboard enemy_pawn_attacks = color == Color::White ? pawn_attacks_black(enemy_pawns)
                                                        : pawn_attacks_white(enemy_pawns);

    Bitboard knights = board.pieces(color, PieceType::Knight);
    while (knights) {
        int sq = pop_lsb(knights);
        int rank = rank_of(sq);
        bool supported = false;
        if (color == Color::White) {
            if (sq - 7 >= 0 && file_of(sq) < 7 && (friendly_pawns & one_bit(sq - 7))) {
                supported = true;
            }
            if (sq - 9 >= 0 && file_of(sq) > 0 && (friendly_pawns & one_bit(sq - 9))) {
                supported = true;
            }
            if (rank >= 4 && supported && !(enemy_pawn_attacks & one_bit(sq))) {
                score += 35;
            }
        } else {
            if (sq + 7 < 64 && file_of(sq) > 0 && (friendly_pawns & one_bit(sq + 7))) {
                supported = true;
            }
            if (sq + 9 < 64 && file_of(sq) < 7 && (friendly_pawns & one_bit(sq + 9))) {
                supported = true;
            }
            if (rank <= 3 && supported && !(enemy_pawn_attacks & one_bit(sq))) {
                score += 35;
            }
        }
        if (file_of(sq) >= 2 && file_of(sq) <= 5 && rank >= 2 && rank <= 5) {
            score += 5;
        }
    }

    Bitboard bishops = board.pieces(color, PieceType::Bishop);
    while (bishops) {
        int sq = pop_lsb(bishops);
        if (file_of(sq) == rank_of(sq) || file_of(sq) + rank_of(sq) == 7) {
            score += 8;
        }
        // Light-squared bishop vs enemy pawns on same color squares penalty
        int color_square = (file_of(sq) + rank_of(sq)) & 1;
        Bitboard mask = color_square ? light_square_mask : dark_square_mask;
        int opposing_pawns_same_color = std::popcount(enemy_pawns & mask);
        score -= opposing_pawns_same_color * 2;
    }

    return color == Color::White ? score : -score;
}

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
    if (auto endgame_eval = evaluate_specialized_endgame(board); endgame_eval.has_value()) {
        return *endgame_eval;
    }

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

    auto white_counts = pawn_file_counts(board, Color::White);
    auto black_counts = pawn_file_counts(board, Color::Black);

    score += evaluate_pawn_structure(board, Color::White, white_counts, black_counts);
    score += evaluate_pawn_structure(board, Color::Black, black_counts, white_counts);
    score += evaluate_king_safety(board, Color::White, white_counts);
    score += evaluate_king_safety(board, Color::Black, black_counts);
    score += evaluate_mobility(board, Color::White);
    score += evaluate_mobility(board, Color::Black);
    score += evaluate_minor_pieces(board, Color::White);
    score += evaluate_minor_pieces(board, Color::Black);

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

    Bitboard white_bishops = board.pieces(Color::White, PieceType::Bishop);
    Bitboard black_bishops = board.pieces(Color::Black, PieceType::Bishop);
    if (std::popcount(white_bishops) == 1 && std::popcount(black_bishops) == 1) {
        int white_sq = bit_scan_forward(white_bishops);
        int black_sq = bit_scan_forward(black_bishops);
        bool white_light = ((file_of(white_sq) + rank_of(white_sq)) & 1) != 0;
        bool black_light = ((file_of(black_sq) + rank_of(black_sq)) & 1) != 0;
        if (white_light != black_light) {
            score /= 2;
        }
    }

    return score;
}

}  // namespace sirio

