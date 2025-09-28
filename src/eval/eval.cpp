#include "engine/eval/eval.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cstdint>
#include <utility>

#include "engine/eval/nnue/psqt.hpp"

namespace engine::eval {

namespace {

struct Score {
    int mg = 0;
    int eg = 0;

    Score& operator+=(const Score& other) {
        mg += other.mg;
        eg += other.eg;
        return *this;
    }
};

constexpr std::array<uint64_t, 8> kFileMasks = {
    0x0101010101010101ULL, 0x0202020202020202ULL, 0x0404040404040404ULL,
    0x0808080808080808ULL, 0x1010101010101010ULL, 0x2020202020202020ULL,
    0x4040404040404040ULL, 0x8080808080808080ULL};

constexpr uint64_t kFileMaskA = kFileMasks[0];
constexpr uint64_t kFileMaskH = kFileMasks[7];

constexpr int kKingShieldMissingMg = 20;
constexpr int kKingShieldMissingEg = 8;
constexpr int kKingShieldAdvanceMg = 6;
constexpr int kKingShieldAdvanceEg = 2;

constexpr int kHalfOpenFileMg = 12;
constexpr int kHalfOpenFileEg = 4;
constexpr int kOpenFileMg = 18;
constexpr int kOpenFileEg = 6;
constexpr int kHeavyFilePressureMg = 8;
constexpr int kHeavyFilePressureEg = 3;

constexpr int kPawnAttackWeightMg = 6;
constexpr int kPawnAttackWeightEg = 2;
constexpr int kKnightAttackWeightMg = 12;
constexpr int kKnightAttackWeightEg = 4;
constexpr int kBishopAttackWeightMg = 10;
constexpr int kBishopAttackWeightEg = 4;
constexpr int kRookAttackWeightMg = 14;
constexpr int kRookAttackWeightEg = 6;
constexpr int kQueenAttackWeightMg = 18;
constexpr int kQueenAttackWeightEg = 8;

constexpr int kIsolatedPawnMg = 12;
constexpr int kIsolatedPawnEg = 10;
constexpr int kDoubledPawnMg = 14;
constexpr int kDoubledPawnEg = 10;
constexpr int kPassedPawnBaseMg = 14;
constexpr int kPassedPawnBaseEg = 24;
constexpr int kPassedPawnAdvanceMg = 4;
constexpr int kPassedPawnAdvanceEg = 6;

inline int file_of(int sq) { return sq & 7; }
inline int rank_of(int sq) { return sq >> 3; }

inline bool on_board(int file, int rank) {
    return file >= 0 && file < 8 && rank >= 0 && rank < 8;
}

inline uint64_t square_bb(int sq) { return 1ULL << sq; }

inline uint64_t shift_north(uint64_t bb) { return bb << 8; }
inline uint64_t shift_south(uint64_t bb) { return bb >> 8; }

inline int pop_lsb(uint64_t& bb) {
#if defined(__GNUG__) || defined(__clang__)
    int sq = __builtin_ctzll(bb);
#else
    int sq = 0;
    uint64_t copy = bb;
    while ((copy & 1ULL) == 0ULL) {
        copy >>= 1ULL;
        ++sq;
    }
#endif
    bb &= bb - 1ULL;
    return sq;
}

inline int first_square(uint64_t bb) {
#if defined(__GNUG__) || defined(__clang__)
    return bb ? __builtin_ctzll(bb) : Board::INVALID_SQUARE;
#else
    if (bb == 0ULL) return Board::INVALID_SQUARE;
    int sq = 0;
    uint64_t copy = bb;
    while ((copy & 1ULL) == 0ULL) {
        copy >>= 1ULL;
        ++sq;
    }
    return sq;
#endif
}

int king_square(bool white, const std::array<uint64_t, Board::PIECE_NB>& bitboards) {
    uint64_t king_bb = white ? bitboards[Board::WHITE_KING] : bitboards[Board::BLACK_KING];
    return first_square(king_bb);
}

const std::array<uint64_t, 64>& knight_attacks_table() {
    static const std::array<uint64_t, 64> table = [] {
        std::array<uint64_t, 64> attacks{};
        for (int sq = 0; sq < 64; ++sq) {
            int file = file_of(sq);
            int rank = rank_of(sq);
            uint64_t mask = 0ULL;
            const std::array<std::pair<int, int>, 8> offsets = {
                std::pair{-2, -1}, std::pair{-2, 1},  std::pair{-1, -2}, std::pair{-1, 2},
                std::pair{1, -2},  std::pair{1, 2},   std::pair{2, -1},  std::pair{2, 1}};
            for (auto [df, dr] : offsets) {
                int nf = file + df;
                int nr = rank + dr;
                if (!on_board(nf, nr)) continue;
                mask |= square_bb(nr * 8 + nf);
            }
            attacks[static_cast<size_t>(sq)] = mask;
        }
        return attacks;
    }();
    return table;
}

const std::array<uint64_t, 64>& king_attacks_table() {
    static const std::array<uint64_t, 64> table = [] {
        std::array<uint64_t, 64> attacks{};
        for (int sq = 0; sq < 64; ++sq) {
            int file = file_of(sq);
            int rank = rank_of(sq);
            uint64_t mask = 0ULL;
            for (int df = -1; df <= 1; ++df) {
                for (int dr = -1; dr <= 1; ++dr) {
                    if (df == 0 && dr == 0) continue;
                    int nf = file + df;
                    int nr = rank + dr;
                    if (!on_board(nf, nr)) continue;
                    mask |= square_bb(nr * 8 + nf);
                }
            }
            attacks[static_cast<size_t>(sq)] = mask;
        }
        return attacks;
    }();
    return table;
}

uint64_t bishop_attacks(int sq, uint64_t occ) {
    uint64_t attacks = 0ULL;
    int file = file_of(sq);
    int rank = rank_of(sq);
    const std::array<std::pair<int, int>, 4> directions = {
        std::pair{1, 1}, std::pair{-1, 1}, std::pair{1, -1}, std::pair{-1, -1}};
    for (auto [df, dr] : directions) {
        int nf = file + df;
        int nr = rank + dr;
        while (on_board(nf, nr)) {
            int target = nr * 8 + nf;
            attacks |= square_bb(target);
            if (occ & square_bb(target)) break;
            nf += df;
            nr += dr;
        }
    }
    return attacks;
}

uint64_t rook_attacks(int sq, uint64_t occ) {
    uint64_t attacks = 0ULL;
    int file = file_of(sq);
    int rank = rank_of(sq);
    const std::array<std::pair<int, int>, 4> directions = {
        std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}};
    for (auto [df, dr] : directions) {
        int nf = file + df;
        int nr = rank + dr;
        while (on_board(nf, nr)) {
            int target = nr * 8 + nf;
            attacks |= square_bb(target);
            if (occ & square_bb(target)) break;
            nf += df;
            nr += dr;
        }
    }
    return attacks;
}

uint64_t pawn_attacks(bool white, uint64_t pawns) {
    if (white) {
        uint64_t left = (pawns << 7) & ~kFileMaskH;
        uint64_t right = (pawns << 9) & ~kFileMaskA;
        return left | right;
    }
    uint64_t left = (pawns >> 9) & ~kFileMaskH;
    uint64_t right = (pawns >> 7) & ~kFileMaskA;
    return left | right;
}

Score kingside_shield_score(uint64_t pawns, bool white) {
    Score score{};
    const std::array<int, 3> files = {5, 6, 7};
    int ideal_rank = white ? 1 : 6;
    for (int file : files) {
        uint64_t mask = kFileMasks[static_cast<size_t>(file)];
        uint64_t pawns_on_file = pawns & mask;
        if (pawns_on_file == 0ULL) {
            score.mg -= kKingShieldMissingMg;
            score.eg -= kKingShieldMissingEg;
            continue;
        }
        int best_rank = white ? 7 : 0;
        uint64_t copy = pawns_on_file;
        while (copy) {
            int sq = pop_lsb(copy);
            int r = rank_of(sq);
            if (white) {
                best_rank = std::min(best_rank, r);
            } else {
                best_rank = std::max(best_rank, r);
            }
        }
        if ((white && best_rank > ideal_rank) || (!white && best_rank < ideal_rank)) {
            int advance = white ? (best_rank - ideal_rank) : (ideal_rank - best_rank);
            score.mg -= advance * kKingShieldAdvanceMg;
            score.eg -= advance * kKingShieldAdvanceEg;
        }
    }
    return score;
}

bool heavy_piece_targets_king(uint64_t pieces, int king_sq, uint64_t occ) {
    uint64_t copy = pieces;
    while (copy) {
        int sq = pop_lsb(copy);
        if (rook_attacks(sq, occ) & square_bb(king_sq)) return true;
    }
    return false;
}

Score king_file_pressure(const Board& board, bool white,
                         const std::array<uint64_t, Board::PIECE_NB>& bitboards) {
    Score score{};
    int king_sq = king_square(white, bitboards);
    if (king_sq == Board::INVALID_SQUARE) return score;

    int king_file = file_of(king_sq);
    uint64_t friendly_pawns = white ? bitboards[Board::WHITE_PAWN]
                                    : bitboards[Board::BLACK_PAWN];
    uint64_t enemy_pawns = white ? bitboards[Board::BLACK_PAWN]
                                 : bitboards[Board::WHITE_PAWN];
    uint64_t enemy_rooks = white ? bitboards[Board::BLACK_ROOK]
                                 : bitboards[Board::WHITE_ROOK];
    uint64_t enemy_queens = white ? bitboards[Board::BLACK_QUEEN]
                                  : bitboards[Board::WHITE_QUEEN];
    uint64_t occ = board.occupancy()[Board::OCC_BOTH];

    for (int df = -1; df <= 1; ++df) {
        int file = king_file + df;
        if (file < 0 || file > 7) continue;
        uint64_t mask = kFileMasks[static_cast<size_t>(file)];
        bool has_friendly = (friendly_pawns & mask) != 0ULL;
        if (has_friendly) continue;
        bool has_enemy = (enemy_pawns & mask) != 0ULL;
        int mg_pen = has_enemy ? kHalfOpenFileMg : kOpenFileMg;
        int eg_pen = has_enemy ? kHalfOpenFileEg : kOpenFileEg;
        bool heavy = heavy_piece_targets_king(enemy_rooks | enemy_queens, king_sq, occ);
        if (heavy) {
            mg_pen += kHeavyFilePressureMg;
            eg_pen += kHeavyFilePressureEg;
        }
        score.mg -= mg_pen;
        score.eg -= eg_pen;
    }
    return score;
}

Score king_attacker_penalty(const Board& board, bool white,
                            const std::array<uint64_t, Board::PIECE_NB>& bitboards) {
    Score score{};
    int king_sq = king_square(white, bitboards);
    if (king_sq == Board::INVALID_SQUARE) return score;
    uint64_t king_bb = square_bb(king_sq);
    uint64_t zone = king_attacks_table()[static_cast<size_t>(king_sq)] | king_bb;
    if (white) {
        zone |= shift_north(zone);
    } else {
        zone |= shift_south(zone);
    }

    uint64_t occ = board.occupancy()[Board::OCC_BOTH];
    uint64_t enemy_pawns = white ? bitboards[Board::BLACK_PAWN]
                                 : bitboards[Board::WHITE_PAWN];
    uint64_t enemy_knights = white ? bitboards[Board::BLACK_KNIGHT]
                                   : bitboards[Board::WHITE_KNIGHT];
    uint64_t enemy_bishops = white ? bitboards[Board::BLACK_BISHOP]
                                   : bitboards[Board::WHITE_BISHOP];
    uint64_t enemy_rooks = white ? bitboards[Board::BLACK_ROOK]
                                 : bitboards[Board::WHITE_ROOK];
    uint64_t enemy_queens = white ? bitboards[Board::BLACK_QUEEN]
                                  : bitboards[Board::WHITE_QUEEN];

    uint64_t copy = enemy_pawns;
    int pawn_attackers = 0;
    while (copy) {
        int sq = pop_lsb(copy);
        uint64_t attacks = pawn_attacks(!white, square_bb(sq));
        if (attacks & zone) ++pawn_attackers;
    }

    copy = enemy_knights;
    int knight_attackers = 0;
    const auto& knight_table = knight_attacks_table();
    while (copy) {
        int sq = pop_lsb(copy);
        if (knight_table[static_cast<size_t>(sq)] & zone) ++knight_attackers;
    }

    copy = enemy_bishops;
    int bishop_attackers = 0;
    while (copy) {
        int sq = pop_lsb(copy);
        if (bishop_attacks(sq, occ) & zone) ++bishop_attackers;
    }

    copy = enemy_rooks;
    int rook_attackers = 0;
    while (copy) {
        int sq = pop_lsb(copy);
        if (rook_attacks(sq, occ) & zone) ++rook_attackers;
    }

    copy = enemy_queens;
    int queen_attackers = 0;
    while (copy) {
        int sq = pop_lsb(copy);
        uint64_t attacks = bishop_attacks(sq, occ) | rook_attacks(sq, occ);
        if (attacks & zone) ++queen_attackers;
    }

    score.mg -= pawn_attackers * kPawnAttackWeightMg;
    score.eg -= pawn_attackers * kPawnAttackWeightEg;
    score.mg -= knight_attackers * kKnightAttackWeightMg;
    score.eg -= knight_attackers * kKnightAttackWeightEg;
    score.mg -= bishop_attackers * kBishopAttackWeightMg;
    score.eg -= bishop_attackers * kBishopAttackWeightEg;
    score.mg -= rook_attackers * kRookAttackWeightMg;
    score.eg -= rook_attackers * kRookAttackWeightEg;
    score.mg -= queen_attackers * kQueenAttackWeightMg;
    score.eg -= queen_attackers * kQueenAttackWeightEg;

    return score;
}

Score pawn_structure_score(bool white, uint64_t pawns, uint64_t enemy_pawns) {
    Score score{};
    for (int file = 0; file < 8; ++file) {
        uint64_t mask = kFileMasks[static_cast<size_t>(file)];
        uint64_t pawns_on_file = pawns & mask;
        int count = std::popcount(pawns_on_file);
        if (count > 1) {
            score.mg -= (count - 1) * kDoubledPawnMg;
            score.eg -= (count - 1) * kDoubledPawnEg;
        }
        uint64_t adj_mask = 0ULL;
        if (file > 0) adj_mask |= kFileMasks[static_cast<size_t>(file - 1)];
        if (file < 7) adj_mask |= kFileMasks[static_cast<size_t>(file + 1)];
        if (pawns_on_file != 0ULL && (pawns & adj_mask) == 0ULL) {
            score.mg -= count * kIsolatedPawnMg;
            score.eg -= count * kIsolatedPawnEg;
        }
    }

    uint64_t copy = pawns;
    while (copy) {
        int sq = pop_lsb(copy);
        int file = file_of(sq);
        int rank = rank_of(sq);
        bool blocked = false;
        for (int df = -1; df <= 1 && !blocked; ++df) {
            int f = file + df;
            if (f < 0 || f > 7) continue;
            int r = rank;
            while (true) {
                if (white) {
                    ++r;
                    if (r >= 8) break;
                } else {
                    --r;
                    if (r < 0) break;
                }
                int target = r * 8 + f;
                if (enemy_pawns & square_bb(target)) {
                    blocked = true;
                    break;
                }
            }
        }
        if (!blocked) {
            int advance = white ? rank : (7 - rank);
            score.mg += kPassedPawnBaseMg + advance * kPassedPawnAdvanceMg;
            score.eg += kPassedPawnBaseEg + advance * kPassedPawnAdvanceEg;
        }
    }

    return score;
}

Score king_safety_score(const Board& board, bool white,
                        const std::array<uint64_t, Board::PIECE_NB>& bitboards) {
    Score score{};
    uint64_t pawns = white ? bitboards[Board::WHITE_PAWN] : bitboards[Board::BLACK_PAWN];
    score += kingside_shield_score(pawns, white);
    score += king_file_pressure(board, white, bitboards);
    score += king_attacker_penalty(board, white, bitboards);
    return score;
}

} // namespace

int evaluate(const Board& board) {
    int mg_score = 0;
    int eg_score = 0;
    int phase = 0;

    const auto& squares = board.squares();
    for (int sq = 0; sq < 64; ++sq) {
        char piece = squares[static_cast<size_t>(sq)];
        if (piece == '.') continue;
        int idx = nnue::piece_index(piece);
        if (idx < 0) continue;
        bool white = std::isupper(static_cast<unsigned char>(piece));
        int table_sq = white ? sq : nnue::mirror_square(sq);
        int mg = nnue::kMgPieceValues[static_cast<size_t>(idx)] +
                 nnue::kMgPst[static_cast<size_t>(idx)][static_cast<size_t>(table_sq)];
        int eg = nnue::kEgPieceValues[static_cast<size_t>(idx)] +
                 nnue::kEgPst[static_cast<size_t>(idx)][static_cast<size_t>(table_sq)];
        if (white) {
            mg_score += mg;
            eg_score += eg;
        } else {
            mg_score -= mg;
            eg_score -= eg;
        }
        phase += nnue::kGamePhaseInc[static_cast<size_t>(idx)];
    }

    const auto& bitboards = board.piece_bitboards();
    Score white_pawn_score =
        pawn_structure_score(true, bitboards[Board::WHITE_PAWN],
                             bitboards[Board::BLACK_PAWN]);
    mg_score += white_pawn_score.mg;
    eg_score += white_pawn_score.eg;
    Score black_pawn_score =
        pawn_structure_score(false, bitboards[Board::BLACK_PAWN],
                             bitboards[Board::WHITE_PAWN]);
    mg_score -= black_pawn_score.mg;
    eg_score -= black_pawn_score.eg;

    Score white_king_score = king_safety_score(board, true, bitboards);
    mg_score += white_king_score.mg;
    eg_score += white_king_score.eg;
    Score black_king_score = king_safety_score(board, false, bitboards);
    mg_score -= black_king_score.mg;
    eg_score -= black_king_score.eg;

    phase = std::clamp(phase, 0, nnue::kGamePhaseMax);
    int eg_phase = nnue::kGamePhaseMax - phase;
    int score = (mg_score * phase + eg_score * eg_phase) / nnue::kGamePhaseMax;
    score += board.white_to_move() ? 10 : -10;
    return board.white_to_move() ? score : -score;
}

} // namespace engine::eval

