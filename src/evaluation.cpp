#include "sirio/evaluation.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <cstdlib>
#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "sirio/bitboard.hpp"
#include "sirio/endgame.hpp"
#include "sirio/nnue/backend.hpp"

namespace sirio {

namespace {

class ClassicalEvaluation : public EvaluationBackend {
public:
    void initialize(const Board &board) override {
        (void)board;
        pawn_cache_.clear();
    }
    void reset(const Board &board) override { initialize(board); }
    void push(const Board &, const std::optional<Move> &, const Board &) override {}
    void pop() override {}
    int evaluate(const Board &board) override;

    [[nodiscard]] std::unique_ptr<EvaluationBackend> clone() const override {
        return std::make_unique<ClassicalEvaluation>(*this);
    }

private:
    struct PawnStructureKey {
        Bitboard white_pawns;
        Bitboard black_pawns;
        bool operator==(const PawnStructureKey &) const = default;
    };

    struct PawnStructureKeyHash {
        std::size_t operator()(const PawnStructureKey &key) const noexcept {
            std::size_t seed = std::hash<Bitboard>{}(key.white_pawns);
            seed ^= std::hash<Bitboard>{}(key.black_pawns) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    struct PawnStructureData {
        std::array<int, 8> white_counts{};
        std::array<int, 8> black_counts{};
        int white_score = 0;
        int black_score = 0;
    };

    std::unordered_map<PawnStructureKey, PawnStructureData, PawnStructureKeyHash> pawn_cache_{};
};

constexpr std::array<int, 6> piece_values_mg = {100, 325, 340, 510, 980, 0};
constexpr std::array<int, 6> piece_values_eg = {100, 310, 320, 520, 1000, 0};
constexpr std::array<int, 6> piece_phase_values = {0, 1, 1, 2, 4, 0};
constexpr int max_game_phase = 24;
constexpr int bishop_pair_bonus_mg = 45;
constexpr int bishop_pair_bonus_eg = 35;
constexpr int endgame_material_threshold = 1300;
constexpr int king_distance_scale = 12;
constexpr int king_corner_scale = 6;
constexpr int king_opposition_bonus = 20;

constexpr std::array<int, 64> pawn_table = {
    0,  0,  0,  0,  0,  0,  0,  0,
    15, 18, 20, 20, 20, 20, 18, 15,
    12, 16, 20, 25, 25, 20, 16, 12,
    8,  12, 18, 30, 30, 18, 12, 8,
    4,  8,  16, 28, 28, 16, 8,  4,
    2,  6,  12, 20, 20, 12, 6,  2,
    0,  0,  4,  8,  8,  4,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0};

constexpr std::array<int, 64> knight_table = {
    -30, -20, -15, -15, -15, -15, -20, -30,
    -20,  -5,   0,   5,   5,   0,  -5, -20,
    -15,   0,  10,  18,  18,  10,   0, -15,
    -15,   5,  18,  24,  24,  18,   5, -15,
    -15,   5,  18,  24,  24,  18,   5, -15,
    -15,   0,  12,  18,  18,  12,   0, -15,
    -20,  -5,   0,   6,   6,   0,  -5, -20,
    -30, -20, -15, -15, -15, -15, -20, -30};

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

constexpr std::array<int, 64> king_table_endgame = {
    -50, -40, -30, -20, -20, -30, -40, -50,
    -30, -20, -10, 0,   0,   -10, -20, -30,
    -30, -10, 20,  30,  30,  20,  -10, -30,
    -30, -10, 30,  40,  40,  30,  -10, -30,
    -30, -10, 30,  40,  40,  30,  -10, -30,
    -30, -10, 20,  30,  30,  20,  -10, -30,
    -30, -30, 0,   0,   0,   0,   -30, -30,
    -50, -40, -30, -20, -20, -30, -40, -50};

const std::array<const std::array<int, 64> *, 6> piece_square_tables_mg = {
    &pawn_table, &knight_table, &bishop_table, &rook_table, &queen_table, &king_table};

const std::array<const std::array<int, 64> *, 6> piece_square_tables_eg = {
    &pawn_table, &knight_table, &bishop_table, &rook_table, &queen_table, &king_table_endgame};

constexpr int weight_scale = 100;
constexpr int pawn_structure_mg_weight = 72;
constexpr int pawn_structure_eg_weight = 102;
constexpr int king_safety_mg_weight = 110;
constexpr int king_safety_eg_weight = 50;
constexpr int mobility_mg_weight = 90;
constexpr int mobility_eg_weight = 100;
constexpr int minor_piece_mg_weight = 95;
constexpr int minor_piece_eg_weight = 105;

constexpr std::array<int, 8> king_attackers_table = {0, 6, 14, 24, 36, 50, 66, 84};

constexpr Bitboard light_square_mask = 0x55AA55AA55AA55AAULL;
constexpr Bitboard dark_square_mask = 0xAA55AA55AA55AA55ULL;

constexpr int backward_pawn_penalty = 18;
constexpr int backward_pawn_rank_scale = 2;
constexpr int connected_passed_bonus = 18;
constexpr int connected_passed_scale = 4;
constexpr int pawn_chain_bonus = 12;
constexpr int bishop_color_pawn_penalty = 6;

constexpr std::array<Bitboard, 8> generate_file_masks() {
    std::array<Bitboard, 8> masks{};
    for (int file = 0; file < 8; ++file) {
        Bitboard mask = 0;
        for (int rank = 0; rank < 8; ++rank) {
            mask |= one_bit(rank * 8 + file);
        }
        masks[static_cast<std::size_t>(file)] = mask;
    }
    return masks;
}

constexpr auto file_masks = generate_file_masks();

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

    Bitboard friendly_pawns = board.pieces(color, PieceType::Pawn);
    Bitboard pawns = friendly_pawns;
    Bitboard enemy_pawns = board.pieces(opposite(color), PieceType::Pawn);
    Bitboard enemy_pawn_attacks =
        color == Color::White ? pawn_attacks_black(enemy_pawns) : pawn_attacks_white(enemy_pawns);
    Bitboard occupancy = board.occupancy();
    Bitboard bishops = board.pieces(color, PieceType::Bishop);
    bool has_light_bishop = (bishops & light_square_mask) != 0;
    bool has_dark_bishop = (bishops & dark_square_mask) != 0;

    auto is_passed_pawn = [&](int pawn_sq) {
        int pawn_file = file_of(pawn_sq);
        int pawn_rank = rank_of(pawn_sq);
        for (int adj = std::max(0, pawn_file - 1); adj <= std::min(7, pawn_file + 1); ++adj) {
            Bitboard mask = 0;
            if (color == Color::White) {
                for (int r = pawn_rank + 1; r < 8; ++r) {
                    mask |= one_bit(r * 8 + adj);
                }
            } else {
                for (int r = pawn_rank - 1; r >= 0; --r) {
                    mask |= one_bit(r * 8 + adj);
                }
            }
            if (enemy_pawns & mask) {
                return false;
            }
        }
        return true;
    };

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

        Bitboard advance_mask = 0;
        if (color == Color::White) {
            for (int r = rank + 1; r < 8; ++r) {
                advance_mask |= one_bit(r * 8 + file);
            }
        } else {
            for (int r = rank - 1; r >= 0; --r) {
                advance_mask |= one_bit(r * 8 + file);
            }
        }

        Bitboard lateral_cover_mask = 0;
        if (file > 0) {
            if (color == Color::White) {
                for (int r = rank; r < 8; ++r) {
                    lateral_cover_mask |= one_bit(r * 8 + (file - 1));
                }
            } else {
                for (int r = rank; r >= 0; --r) {
                    lateral_cover_mask |= one_bit(r * 8 + (file - 1));
                }
            }
        }
        if (file < 7) {
            if (color == Color::White) {
                for (int r = rank; r < 8; ++r) {
                    lateral_cover_mask |= one_bit(r * 8 + (file + 1));
                }
            } else {
                for (int r = rank; r >= 0; --r) {
                    lateral_cover_mask |= one_bit(r * 8 + (file + 1));
                }
            }
        }

        int forward_sq = -1;
        Bitboard forward_bit = 0;
        if (color == Color::White && rank < 7) {
            forward_sq = sq + 8;
            forward_bit = one_bit(forward_sq);
        } else if (color == Color::Black && rank > 0) {
            forward_sq = sq - 8;
            forward_bit = one_bit(forward_sq);
        }

        bool enemy_controls_forward = (forward_bit != 0) && (enemy_pawn_attacks & forward_bit);
        bool enemy_blocking_forward = false;
        if ((forward_bit != 0) && (occupancy & forward_bit)) {
            auto piece = board.piece_at(forward_sq);
            enemy_blocking_forward = piece && piece->first != color;
        }

        int relative_rank = color == Color::White ? rank : (7 - rank);
        bool has_lateral_support = (friendly_pawns & lateral_cover_mask) != 0;
        if (!has_lateral_support && forward_bit != 0 && !(friendly_pawns & advance_mask) &&
            (enemy_controls_forward || enemy_blocking_forward)) {
            score -= backward_pawn_penalty + relative_rank * backward_pawn_rank_scale;
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
            int base_bonus = 28 + relative_rank * 12;
            bool connected_passed = false;
            Bitboard connected_candidates = friendly_pawns & lateral_cover_mask;
            Bitboard tmp_candidates = connected_candidates;
            while (tmp_candidates) {
                int other_sq = pop_lsb(tmp_candidates);
                if (is_passed_pawn(other_sq)) {
                    connected_passed = true;
                    break;
                }
            }

            score += base_bonus;
            if (connected_passed) {
                score += connected_passed_bonus + relative_rank * connected_passed_scale;
            }
        }

        if (relative_rank == 3 || relative_rank == 4) {
            bool in_chain = false;
            if (color == Color::White) {
                if (file > 0 && rank > 0 && (friendly_pawns & one_bit(sq - 9))) {
                    in_chain = true;
                }
                if (file < 7 && rank > 0 && (friendly_pawns & one_bit(sq - 7))) {
                    in_chain = true;
                }
            } else {
                if (file > 0 && rank < 7 && (friendly_pawns & one_bit(sq + 7))) {
                    in_chain = true;
                }
                if (file < 7 && rank < 7 && (friendly_pawns & one_bit(sq + 9))) {
                    in_chain = true;
                }
            }
            if (in_chain) {
                score += pawn_chain_bonus + relative_rank;
            }
        }

        bool is_light_square = ((file + rank) & 1) != 0;
        if ((is_light_square && has_light_bishop) || (!is_light_square && has_dark_bishop)) {
            score -= bishop_color_pawn_penalty;
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

    Bitboard king_zone = king_attacks(king_sq) | one_bit(king_sq);
    Color enemy = opposite(color);
    Bitboard occupancy = board.occupancy();
    Bitboard friendly_pawns = board.pieces(color, PieceType::Pawn);
    auto enemy_counts = pawn_file_counts(board, enemy);

    int attackers = 0;

    Bitboard enemy_knights = board.pieces(enemy, PieceType::Knight);
    int attack_penalty = 0;
    while (enemy_knights) {
        int sq = pop_lsb(enemy_knights);
        Bitboard attacks = knight_attacks(sq) & king_zone;
        int hits = std::popcount(attacks);
        if (hits != 0) {
            attack_penalty += hits * 6;
            ++attackers;
        }
    }

    Bitboard enemy_bishops = board.pieces(enemy, PieceType::Bishop);
    Bitboard tmp = enemy_bishops;
    while (tmp) {
        int sq = pop_lsb(tmp);
        Bitboard attacks = bishop_attacks(sq, occupancy) & king_zone;
        int hits = std::popcount(attacks);
        if (hits != 0) {
            attack_penalty += hits * 5;
            ++attackers;
        }
    }

    Bitboard enemy_rooks = board.pieces(enemy, PieceType::Rook);
    tmp = enemy_rooks;
    while (tmp) {
        int sq = pop_lsb(tmp);
        Bitboard attacks = rook_attacks(sq, occupancy) & king_zone;
        int hits = std::popcount(attacks);
        if (hits != 0) {
            attack_penalty += hits * 4;
            ++attackers;
        }
    }

    Bitboard enemy_queens = board.pieces(enemy, PieceType::Queen);
    tmp = enemy_queens;
    while (tmp) {
        int sq = pop_lsb(tmp);
        Bitboard bishop_hits = bishop_attacks(sq, occupancy) & king_zone;
        Bitboard rook_hits = rook_attacks(sq, occupancy) & king_zone;
        int hits = std::popcount(bishop_hits) + std::popcount(rook_hits);
        if (hits != 0) {
            attack_penalty += std::popcount(bishop_hits) * 5;
            attack_penalty += std::popcount(rook_hits) * 4;
            ++attackers;
        }
    }

    Bitboard enemy_pawns = board.pieces(enemy, PieceType::Pawn);
    Bitboard pawn_attacks = enemy == Color::White ? pawn_attacks_white(enemy_pawns)
                                                 : pawn_attacks_black(enemy_pawns);
    int pawn_hits = std::popcount(pawn_attacks & king_zone);
    if (pawn_hits != 0) {
        attack_penalty += pawn_hits * 7;
        ++attackers;
    }

    int advanced_pawn_penalty = 0;
    Bitboard advanced_pawns = enemy_pawns;
    while (advanced_pawns) {
        int sq = pop_lsb(advanced_pawns);
        int file = file_of(sq);
        if (std::abs(file - king_file) > 1) {
            continue;
        }
        int rank = rank_of(sq);
        bool advanced = false;
        if (color == Color::White) {
            int limit = std::min(7, king_rank + 2);
            advanced = rank <= limit;
        } else {
            int limit = std::max(0, king_rank - 2);
            advanced = rank >= limit;
        }
        if (!advanced) {
            continue;
        }
        int distance = std::abs(rank - king_rank);
        int proximity_bonus = std::max(0, 3 - distance);
        advanced_pawn_penalty += 12 + proximity_bonus * 3;
    }

    Bitboard enemy_heavy = enemy_rooks | enemy_queens;
    int heavy_ray_penalty = 0;
    int heavy_ray_attackers = 0;
    Bitboard heavy_tmp = enemy_heavy;
    while (heavy_tmp) {
        int sq = pop_lsb(heavy_tmp);
        int file = file_of(sq);
        int rank = rank_of(sq);
        int file_diff = king_file - file;
        int rank_diff = king_rank - rank;
        if (file_diff != 0 && rank_diff != 0) {
            continue;
        }
        int file_step = (file_diff == 0) ? 0 : (file_diff > 0 ? 1 : -1);
        int rank_step = (rank_diff == 0) ? 0 : (rank_diff > 0 ? 1 : -1);
        int f = file + file_step;
        int r = rank + rank_step;
        int friendly_blockers = 0;
        int enemy_blockers = 0;
        bool pawn_blocker = false;
        int distance = 0;
        while (f >= 0 && f < 8 && r >= 0 && r < 8) {
            if (f == king_file && r == king_rank) {
                break;
            }
            int index = r * 8 + f;
            Bitboard bit = one_bit(index);
            if (occupancy & bit) {
                auto occupant = board.piece_at(index);
                if (occupant) {
                    if (occupant->first == color) {
                        ++friendly_blockers;
                        pawn_blocker = pawn_blocker || occupant->second == PieceType::Pawn;
                    } else {
                        ++enemy_blockers;
                    }
                }
            }
            ++distance;
            if (friendly_blockers + enemy_blockers > 2) {
                break;
            }
            f += file_step;
            r += rank_step;
        }
        if (f == king_file && r == king_rank && enemy_blockers == 0) {
            if (friendly_blockers == 0) {
                heavy_ray_penalty += 20 + std::max(0, 3 - distance) * 4;
            } else if (friendly_blockers == 1) {
                int blocker_penalty = pawn_blocker ? 18 : 12;
                heavy_ray_penalty += blocker_penalty + std::max(0, 2 - distance) * 3;
                ++heavy_ray_attackers;
            }
        }
    }
    attackers += heavy_ray_attackers;

    int defender_bonus = 0;
    Bitboard friendly_heavy = board.pieces(color, PieceType::Rook) | board.pieces(color, PieceType::Queen);
    Bitboard friendly_tmp = friendly_heavy;
    while (friendly_tmp) {
        int sq = pop_lsb(friendly_tmp);
        int file_diff = std::abs(file_of(sq) - king_file);
        int rank_diff = std::abs(rank_of(sq) - king_rank);
        int distance = std::max(file_diff, rank_diff);
        if (distance > 2) {
            continue;
        }
        int proximity = std::max(0, 2 - distance);
        defender_bonus += 10 + proximity * 4;
        if (file_diff <= 1 && rank_diff <= 1) {
            defender_bonus += 4;
        }
    }

    Bitboard friendly_pawn_attacks =
        color == Color::White ? pawn_attacks_white(friendly_pawns) : pawn_attacks_black(friendly_pawns);
    int weak_square_penalty = 0;
    int forward_rank = color == Color::White ? king_rank + 1 : king_rank - 1;
    if (forward_rank >= 0 && forward_rank < 8) {
        for (int file_offset = -1; file_offset <= 1; ++file_offset) {
            int file = king_file + file_offset;
            if (file < 0 || file > 7) {
                continue;
            }
            int sq = forward_rank * 8 + file;
            auto occupant = board.piece_at(sq);
            if (occupant && occupant->first == color && occupant->second == PieceType::Pawn) {
                continue;
            }
            Bitboard mask = one_bit(sq);
            bool pawn_supported = (friendly_pawn_attacks & mask) != 0;
            if (!board.is_square_attacked(sq, enemy)) {
                continue;
            }
            int penalty = 6;
            if (!pawn_supported) {
                penalty += 4;
            }
            if (!board.is_square_attacked(sq, color)) {
                penalty += 6;
            }
            if (!occupant.has_value()) {
                penalty += 2;
            }
            weak_square_penalty += penalty;
        }
    }

    int castle_file_penalty = 0;
    int castle_attackers = 0;
    std::array<int, 3> castle_files{};
    int castle_file_count = 0;
    auto push_castle_file = [&](int file) {
        if (file < 0 || file > 7) {
            return;
        }
        auto end = castle_files.begin() + castle_file_count;
        if (std::find(castle_files.begin(), end, file) != end) {
            return;
        }
        castle_files[castle_file_count++] = file;
    };

    bool on_home_rank = (color == Color::White) ? (king_rank <= 1) : (king_rank >= 6);
    if (on_home_rank) {
        if (king_file >= 5) {
            push_castle_file(king_file);
            push_castle_file(king_file + 1);
            push_castle_file(king_file - 1);
        } else if (king_file <= 2) {
            push_castle_file(king_file);
            push_castle_file(king_file - 1);
            push_castle_file(king_file + 1);
        } else {
            push_castle_file(king_file - 1);
            push_castle_file(king_file);
            push_castle_file(king_file + 1);
        }
    } else {
        push_castle_file(king_file - 1);
        push_castle_file(king_file);
        push_castle_file(king_file + 1);
    }

    for (int i = 0; i < castle_file_count; ++i) {
        int file = castle_files[static_cast<std::size_t>(i)];
        int friendly_on_file = friendly_counts[static_cast<std::size_t>(file)];
        int enemy_on_file = enemy_counts[static_cast<std::size_t>(file)];
        Bitboard file_mask = file_masks[static_cast<std::size_t>(file)];
        bool has_home_pawn = false;
        if (friendly_on_file > 0) {
            int home_rank = color == Color::White ? 1 : 6;
            int home_square = home_rank * 8 + file;
            auto occupant = board.piece_at(home_square);
            if (occupant && occupant->first == color && occupant->second == PieceType::Pawn) {
                has_home_pawn = true;
            }
        }

        if (friendly_on_file == 0) {
            castle_file_penalty += enemy_on_file == 0 ? 22 : 16;
        } else if (!has_home_pawn) {
            castle_file_penalty += 8;
        }

        Bitboard heavy_on_file = enemy_heavy & file_mask;
        if (heavy_on_file) {
            Bitboard tmp_heavy = heavy_on_file;
            bool hits_zone = false;
            while (tmp_heavy) {
                int sq = pop_lsb(tmp_heavy);
                if (rook_attacks(sq, occupancy) & king_zone) {
                    hits_zone = true;
                    break;
                }
            }
            if (hits_zone) {
                castle_file_penalty += 6;
            } else if (friendly_on_file == 0) {
                castle_file_penalty += 4;
                ++castle_attackers;
            }
        }

        int forward_rank = color == Color::White ? king_rank + 1 : king_rank - 1;
        if (forward_rank >= 0 && forward_rank < 8) {
            int front_square = forward_rank * 8 + file;
            if (!board.piece_at(front_square)) {
                Bitboard pressure = rook_attacks(front_square, occupancy) & enemy_heavy;
                if (pressure) {
                    castle_file_penalty += 4;
                }
            }
        }
    }

    attackers += castle_attackers;

    bool king_on_dark = ((king_file + king_rank) & 1) != 0;
    bool has_dark_bishop = (board.pieces(color, PieceType::Bishop) & dark_square_mask) != 0;
    int dark_square_penalty = 0;
    if (king_on_dark && !has_dark_bishop) {
        Bitboard diagonal_attackers =
            board.pieces(enemy, PieceType::Bishop) | board.pieces(enemy, PieceType::Queen);
        Bitboard tmp_attackers = diagonal_attackers;
        Bitboard dark_zone = king_zone & dark_square_mask;
        while (tmp_attackers) {
            int sq = pop_lsb(tmp_attackers);
            Bitboard attacks = bishop_attacks(sq, occupancy);
            if ((attacks & dark_zone) != 0) {
                dark_square_penalty += 10;
                if (attacks & one_bit(king_sq)) {
                    dark_square_penalty += 8;
                }
            }
        }
    }

    score -= attack_penalty;
    score -= dark_square_penalty;
    score -= advanced_pawn_penalty;
    score -= heavy_ray_penalty;
    score -= weak_square_penalty;
    score -= castle_file_penalty;
    score -= king_attackers_table[std::min(attackers, static_cast<int>(king_attackers_table.size() - 1))];
    score += defender_bonus;
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

int ClassicalEvaluation::evaluate(const Board &board) {
    if (auto endgame_eval = evaluate_specialized_endgame(board); endgame_eval.has_value()) {
        return *endgame_eval;
    }

    int mg_score = 0;
    int eg_score = 0;
    int material_white = 0;
    int material_black = 0;
    int game_phase = 0;

    auto scale_term = [](int value, int weight) {
        int scaled = value * weight;
        if (scaled >= 0) {
            return (scaled + weight_scale / 2) / weight_scale;
        }
        return (scaled - weight_scale / 2) / weight_scale;
    };

    for (int color_index = 0; color_index < 2; ++color_index) {
        Color color = color_index == 0 ? Color::White : Color::Black;
        for (std::size_t piece_index = 0; piece_index < piece_values_mg.size(); ++piece_index) {
            PieceType type = static_cast<PieceType>(piece_index);
            Bitboard pieces = board.pieces(color, type);
            while (pieces) {
                int square = pop_lsb(pieces);
                int base_value_mg = piece_values_mg[piece_index];
                int base_value_eg = piece_values_eg[piece_index];
                int table_index = color == Color::White ? square : mirror_square(square);
                int ps_value_mg = (*piece_square_tables_mg[piece_index])[table_index];
                int ps_value_eg = (*piece_square_tables_eg[piece_index])[table_index];
                int total_mg = base_value_mg + ps_value_mg;
                int total_eg = base_value_eg + ps_value_eg;
                if (color == Color::White) {
                    mg_score += total_mg;
                    eg_score += total_eg;
                    material_white += base_value_mg;
                } else {
                    mg_score -= total_mg;
                    eg_score -= total_eg;
                    material_black += base_value_mg;
                }
                game_phase += piece_phase_values[piece_index];
            }
        }
    }

    if (board.has_bishop_pair(Color::White)) {
        mg_score += bishop_pair_bonus_mg;
        eg_score += bishop_pair_bonus_eg;
    }
    if (board.has_bishop_pair(Color::Black)) {
        mg_score -= bishop_pair_bonus_mg;
        eg_score -= bishop_pair_bonus_eg;
    }

    Bitboard white_pawns = board.pieces(Color::White, PieceType::Pawn);
    Bitboard black_pawns = board.pieces(Color::Black, PieceType::Pawn);

    PawnStructureKey key{white_pawns, black_pawns};
    auto cache_it = pawn_cache_.find(key);
    if (cache_it == pawn_cache_.end()) {
        PawnStructureData data;
        data.white_counts = pawn_file_counts(board, Color::White);
        data.black_counts = pawn_file_counts(board, Color::Black);
        data.white_score =
            evaluate_pawn_structure(board, Color::White, data.white_counts, data.black_counts);
        data.black_score =
            evaluate_pawn_structure(board, Color::Black, data.black_counts, data.white_counts);
        cache_it = pawn_cache_.emplace(key, std::move(data)).first;
    }

    const auto &white_counts = cache_it->second.white_counts;
    const auto &black_counts = cache_it->second.black_counts;

    int pawn_structure_white = cache_it->second.white_score;
    int pawn_structure_black = cache_it->second.black_score;
    mg_score += scale_term(pawn_structure_white, pawn_structure_mg_weight);
    eg_score += scale_term(pawn_structure_white, pawn_structure_eg_weight);
    mg_score += scale_term(pawn_structure_black, pawn_structure_mg_weight);
    eg_score += scale_term(pawn_structure_black, pawn_structure_eg_weight);

    int king_safety_white = evaluate_king_safety(board, Color::White, white_counts);
    int king_safety_black = evaluate_king_safety(board, Color::Black, black_counts);
    mg_score += scale_term(king_safety_white, king_safety_mg_weight);
    eg_score += scale_term(king_safety_white, king_safety_eg_weight);
    mg_score += scale_term(king_safety_black, king_safety_mg_weight);
    eg_score += scale_term(king_safety_black, king_safety_eg_weight);

    int mobility_white = evaluate_mobility(board, Color::White);
    int mobility_black = evaluate_mobility(board, Color::Black);
    mg_score += scale_term(mobility_white, mobility_mg_weight);
    eg_score += scale_term(mobility_white, mobility_eg_weight);
    mg_score += scale_term(mobility_black, mobility_mg_weight);
    eg_score += scale_term(mobility_black, mobility_eg_weight);

    int minor_white = evaluate_minor_pieces(board, Color::White);
    int minor_black = evaluate_minor_pieces(board, Color::Black);
    mg_score += scale_term(minor_white, minor_piece_mg_weight);
    eg_score += scale_term(minor_white, minor_piece_eg_weight);
    mg_score += scale_term(minor_black, minor_piece_mg_weight);
    eg_score += scale_term(minor_black, minor_piece_eg_weight);

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
            eg_score += closeness * king_distance_scale;
            int corner_distance = king_corner_distance_table[static_cast<std::size_t>(black_king)];
            eg_score += (7 - corner_distance) * king_corner_scale;
            int friendly_corner_distance =
                king_corner_distance_table[static_cast<std::size_t>(white_king)];
            eg_score -= friendly_corner_distance * (king_corner_scale / 2);
            int file_diff = std::abs((white_king % 8) - (black_king % 8));
            int rank_diff = std::abs((white_king / 8) - (black_king / 8));
            if ((file_diff == 0 || rank_diff == 0) && ((distance & 1) == 1)) {
                eg_score += king_opposition_bonus;
            }
        } else if (advantage < 0) {
            eg_score -= closeness * king_distance_scale;
            int corner_distance = king_corner_distance_table[static_cast<std::size_t>(white_king)];
            eg_score -= (7 - corner_distance) * king_corner_scale;
            int friendly_corner_distance =
                king_corner_distance_table[static_cast<std::size_t>(black_king)];
            eg_score += friendly_corner_distance * (king_corner_scale / 2);
            int file_diff = std::abs((white_king % 8) - (black_king % 8));
            int rank_diff = std::abs((white_king / 8) - (black_king / 8));
            if ((file_diff == 0 || rank_diff == 0) && ((distance & 1) == 1)) {
                eg_score -= king_opposition_bonus;
            }
        }
    }

    int phase = std::clamp(game_phase, 0, max_game_phase);
    int combined = mg_score * phase + eg_score * (max_game_phase - phase);
    if (combined >= 0) {
        combined += max_game_phase / 2;
    } else {
        combined -= max_game_phase / 2;
    }
    int score = max_game_phase != 0 ? combined / max_game_phase : 0;

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

std::unique_ptr<EvaluationBackend> make_classical_evaluation() {
    return std::make_unique<ClassicalEvaluation>();
}

std::unique_ptr<EvaluationBackend> make_nnue_evaluation(
    const nnue::MultiNetworkConfig &config, std::string *error_message) {
    if (config.primary_path.empty()) {
        if (error_message) {
            *error_message = "NNUE file path is empty";
        }
        return nullptr;
    }
    std::string local_error;
    auto primary = std::make_unique<nnue::SingleNetworkBackend>();
    if (!primary->load(config.primary_path, &local_error)) {
        if (error_message) {
            *error_message = local_error;
        }
        return nullptr;
    }
    if (config.secondary_path.empty()) {
        return primary;
    }

    auto secondary = std::make_unique<nnue::SingleNetworkBackend>();
    if (!secondary->load(config.secondary_path, &local_error)) {
        if (error_message) {
            *error_message = local_error;
        }
        return nullptr;
    }

    return std::make_unique<nnue::MultiNetworkBackend>(std::move(primary), std::move(secondary),
                                                       config.policy, config.phase_threshold);
}

std::unique_ptr<EvaluationBackend> make_nnue_evaluation(const std::string &path,
                                                        std::string *error_message) {
    nnue::MultiNetworkConfig config;
    config.primary_path = path;
    config.policy = nnue::NetworkSelectionPolicy::Material;
    config.phase_threshold = 0;
    return make_nnue_evaluation(config, error_message);
}

namespace {

struct EvaluationGlobalState {
    std::mutex mutex;
    std::unique_ptr<EvaluationBackend> prototype;
    std::atomic<std::uint64_t> generation{1};
};

EvaluationGlobalState &global_state() {
    static EvaluationGlobalState state;
    return state;
}

struct EvaluationThreadState {
    std::unique_ptr<EvaluationBackend> backend;
    bool initialized = false;
    int stack_depth = 0;
    bool notifications_enabled = false;
    std::uint64_t generation = 0;
    nnue::ThreadAccumulator nnue_primary_accumulator;
    nnue::ThreadAccumulator nnue_secondary_accumulator;
};

EvaluationThreadState &thread_state() {
    thread_local EvaluationThreadState state;
    return state;
}

void attach_thread_accumulators(EvaluationThreadState &state) {
    if (!state.backend) {
        return;
    }
    if (auto *multi = dynamic_cast<nnue::MultiNetworkBackend *>(state.backend.get())) {
        multi->set_thread_accumulators(&state.nnue_primary_accumulator,
                                       &state.nnue_secondary_accumulator);
    } else if (auto *single = dynamic_cast<nnue::SingleNetworkBackend *>(state.backend.get())) {
        single->set_thread_accumulator(&state.nnue_primary_accumulator);
    }
}

void ensure_global_backend() {
    EvaluationGlobalState &global = global_state();
    if (!global.prototype) {
        std::lock_guard lock(global.mutex);
        if (!global.prototype) {
            global.prototype = make_classical_evaluation();
            global.generation.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

void ensure_thread_backend() {
    ensure_global_backend();
    EvaluationGlobalState &global = global_state();
    EvaluationThreadState &state = thread_state();
    std::uint64_t current_generation = global.generation.load(std::memory_order_acquire);
    if (!state.backend || state.generation != current_generation) {
        std::lock_guard lock(global.mutex);
        if (!global.prototype) {
            global.prototype = make_classical_evaluation();
            current_generation = global.generation.fetch_add(1, std::memory_order_relaxed) + 1;
        } else {
            current_generation = global.generation.load(std::memory_order_relaxed);
        }
        state.backend = global.prototype->clone();
        state.initialized = false;
        state.stack_depth = 0;
        state.notifications_enabled = false;
        state.generation = current_generation;
        state.nnue_primary_accumulator.reset();
        state.nnue_secondary_accumulator.reset();
        attach_thread_accumulators(state);
    }
}

void ensure_initialized(const Board &board) {
    ensure_thread_backend();
    EvaluationThreadState &state = thread_state();
    if (!state.initialized) {
        state.backend->initialize(board);
        state.initialized = true;
        state.stack_depth = 1;
        state.notifications_enabled = true;
    }
}

}  // namespace

void set_evaluation_backend(std::unique_ptr<EvaluationBackend> backend) {
    if (!backend) {
        backend = make_classical_evaluation();
    }
    EvaluationGlobalState &global = global_state();
    {
        std::lock_guard lock(global.mutex);
        global.prototype = std::move(backend);
        global.generation.fetch_add(1, std::memory_order_release);
    }
    EvaluationThreadState &state = thread_state();
    state.backend.reset();
    state.initialized = false;
    state.stack_depth = 0;
    state.notifications_enabled = false;
    state.generation = 0;
    state.nnue_primary_accumulator.reset();
    state.nnue_secondary_accumulator.reset();
}

void use_classical_evaluation() {
    set_evaluation_backend(make_classical_evaluation());
}

EvaluationBackend &active_evaluation_backend() {
    ensure_thread_backend();
    return *thread_state().backend;
}

void initialize_evaluation(const Board &board) {
    ensure_thread_backend();
    EvaluationThreadState &state = thread_state();
    if (!state.initialized) {
        state.backend->initialize(board);
        state.initialized = true;
    } else {
        state.backend->reset(board);
    }
    state.stack_depth = 1;
    state.notifications_enabled = true;
}

void push_evaluation_state(const Board &previous, const std::optional<Move> &move,
                           const Board &current) {
    ensure_thread_backend();
    EvaluationThreadState &state = thread_state();
    if (!state.notifications_enabled) {
        return;
    }
    ensure_initialized(previous);
    state.backend->push(previous, move, current);
    ++state.stack_depth;
}

void pop_evaluation_state() {
    ensure_thread_backend();
    EvaluationThreadState &state = thread_state();
    if (!state.notifications_enabled) {
        return;
    }
    if (state.stack_depth <= 1) {
        return;
    }
    state.backend->pop();
    --state.stack_depth;
}

void notify_position_initialization(const Board &board) {
    ensure_thread_backend();
    EvaluationThreadState &state = thread_state();
    if (!state.notifications_enabled) {
        return;
    }
    initialize_evaluation(board);
}

void notify_move_applied(const Board &previous, const std::optional<Move> &move,
                         const Board &current) {
    ensure_thread_backend();
    EvaluationThreadState &state = thread_state();
    if (!state.notifications_enabled) {
        return;
    }
    push_evaluation_state(previous, move, current);
}

int evaluate(const Board &board) {
    ensure_initialized(board);
    return thread_state().backend->evaluate(board);
}

}  // namespace sirio

