#include "engine/eval/see.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>

namespace engine::eval {
namespace {

constexpr int WHITE = 0;
constexpr int BLACK = 1;

constexpr uint64_t bit(int sq) { return 1ULL << sq; }

inline int file_of(int sq) { return sq & 7; }
inline int rank_of(int sq) { return sq >> 3; }

uint64_t pawn_attackers_mask(int sq, bool by_white) {
    uint64_t mask = 0ULL;
    int file = file_of(sq);
    int rank = rank_of(sq);
    if (by_white) {
        if (file < 7 && rank > 0) mask |= bit(sq - 7);
        if (file > 0 && rank > 0) mask |= bit(sq - 9);
    } else {
        if (file < 7 && rank < 7) mask |= bit(sq + 9);
        if (file > 0 && rank < 7) mask |= bit(sq + 7);
    }
    return mask;
}

uint64_t knight_attacks(int sq) {
    static const std::array<std::pair<int, int>, 8> offsets{{{1, 2},  {2, 1},  {2, -1}, {1, -2},
                                                             {-1, -2}, {-2, -1}, {-2, 1},  {-1, 2}}};
    static std::array<uint64_t, 64> table = [] {
        std::array<uint64_t, 64> out{};
        for (int s = 0; s < 64; ++s) {
            int file = file_of(s);
            int rank = rank_of(s);
            uint64_t mask = 0ULL;
            for (auto [df, dr] : offsets) {
                int nf = file + df;
                int nr = rank + dr;
                if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) {
                    mask |= bit(nr * 8 + nf);
                }
            }
            out[static_cast<size_t>(s)] = mask;
        }
        return out;
    }();
    return table[static_cast<size_t>(sq)];
}

uint64_t king_attacks(int sq) {
    static const std::array<std::pair<int, int>, 8> offsets{{{1, 0},  {1, 1},  {0, 1},
                                                             {-1, 1}, {-1, 0}, {-1, -1},
                                                             {0, -1}, {1, -1}}};
    static std::array<uint64_t, 64> table = [] {
        std::array<uint64_t, 64> out{};
        for (int s = 0; s < 64; ++s) {
            int file = file_of(s);
            int rank = rank_of(s);
            uint64_t mask = 0ULL;
            for (auto [df, dr] : offsets) {
                int nf = file + df;
                int nr = rank + dr;
                if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) {
                    mask |= bit(nr * 8 + nf);
                }
            }
            out[static_cast<size_t>(s)] = mask;
        }
        return out;
    }();
    return table[static_cast<size_t>(sq)];
}

uint64_t sliding_attacks(int sq, uint64_t occ,
                         const std::array<std::pair<int, int>, 4>& dirs) {
    uint64_t attacks = 0ULL;
    int file = file_of(sq);
    int rank = rank_of(sq);
    for (auto [df, dr] : dirs) {
        int nf = file + df;
        int nr = rank + dr;
        while (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) {
            int target = nr * 8 + nf;
            attacks |= bit(target);
            if (occ & bit(target)) break;
            nf += df;
            nr += dr;
        }
    }
    return attacks;
}

uint64_t bishop_attacks(int sq, uint64_t occ) {
    static const std::array<std::pair<int, int>, 4> dirs{{{1, 1}, {-1, 1}, {1, -1}, {-1, -1}}};
    return sliding_attacks(sq, occ, dirs);
}

uint64_t rook_attacks(int sq, uint64_t occ) {
    static const std::array<std::pair<int, int>, 4> dirs{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
    return sliding_attacks(sq, occ, dirs);
}

std::array<uint64_t, 2> attackers_to(int sq, uint64_t occ,
                                     const std::array<std::array<uint64_t, 6>, 2>& pieces) {
    std::array<uint64_t, 2> attackers{};
    uint64_t pawn_mask_white = pawn_attackers_mask(sq, true);
    uint64_t pawn_mask_black = pawn_attackers_mask(sq, false);
    attackers[WHITE] |= pawn_mask_white & pieces[WHITE][0];
    attackers[BLACK] |= pawn_mask_black & pieces[BLACK][0];

    uint64_t knight_mask = knight_attacks(sq);
    attackers[WHITE] |= knight_mask & pieces[WHITE][1];
    attackers[BLACK] |= knight_mask & pieces[BLACK][1];

    uint64_t bishop_mask = bishop_attacks(sq, occ);
    attackers[WHITE] |= bishop_mask & (pieces[WHITE][2] | pieces[WHITE][4]);
    attackers[BLACK] |= bishop_mask & (pieces[BLACK][2] | pieces[BLACK][4]);

    uint64_t rook_mask = rook_attacks(sq, occ);
    attackers[WHITE] |= rook_mask & (pieces[WHITE][3] | pieces[WHITE][4]);
    attackers[BLACK] |= rook_mask & (pieces[BLACK][3] | pieces[BLACK][4]);

    uint64_t king_mask = king_attacks(sq);
    attackers[WHITE] |= king_mask & pieces[WHITE][5];
    attackers[BLACK] |= king_mask & pieces[BLACK][5];
    return attackers;
}

int piece_type_from_char(char piece) {
    switch (std::tolower(static_cast<unsigned char>(piece))) {
    case 'p': return 0;
    case 'n': return 1;
    case 'b': return 2;
    case 'r': return 3;
    case 'q': return 4;
    case 'k': return 5;
    default: return -1;
    }
}

int piece_value_from_type(int type) {
    if (type < 0 || type >= static_cast<int>(kSeePieceValues.size())) return 0;
    return kSeePieceValues[static_cast<size_t>(type)];
}

int piece_value(char piece) {
    int type = piece_type_from_char(piece);
    return piece_value_from_type(type);
}

char promotion_piece_from_code(int code, bool white) {
    switch (code) {
    case 1: return white ? 'N' : 'n';
    case 2: return white ? 'B' : 'b';
    case 3: return white ? 'R' : 'r';
    case 4: return white ? 'Q' : 'q';
    default: return white ? 'P' : 'p';
    }
}

} // namespace

int see(const Board& board, Move move) {
    int from = move_from(move);
    int to = move_to(move);
    char moving_piece = board.piece_on(from);
    bool en_passant = move_is_enpassant(move);
    bool is_capture = move_is_capture(move) || en_passant;
    if (!is_capture) return 0;

    char captured_piece = en_passant ? (std::isupper(static_cast<unsigned char>(moving_piece)) ? 'p' : 'P')
                                     : board.piece_on(to);
    if (captured_piece == '.') return 0;

    bool moving_white = std::isupper(static_cast<unsigned char>(moving_piece));
    int promo = move_promo(move);
    char promoted_piece = moving_piece;
    if (promo != 0 && std::tolower(static_cast<unsigned char>(moving_piece)) == 'p') {
        promoted_piece = promotion_piece_from_code(promo, moving_white);
    }

    const auto& bitboards = board.piece_bitboards();
    std::array<std::array<uint64_t, 6>, 2> pieces{};
    pieces[WHITE][0] = bitboards[Board::WHITE_PAWN];
    pieces[WHITE][1] = bitboards[Board::WHITE_KNIGHT];
    pieces[WHITE][2] = bitboards[Board::WHITE_BISHOP];
    pieces[WHITE][3] = bitboards[Board::WHITE_ROOK];
    pieces[WHITE][4] = bitboards[Board::WHITE_QUEEN];
    pieces[WHITE][5] = bitboards[Board::WHITE_KING];
    pieces[BLACK][0] = bitboards[Board::BLACK_PAWN];
    pieces[BLACK][1] = bitboards[Board::BLACK_KNIGHT];
    pieces[BLACK][2] = bitboards[Board::BLACK_BISHOP];
    pieces[BLACK][3] = bitboards[Board::BLACK_ROOK];
    pieces[BLACK][4] = bitboards[Board::BLACK_QUEEN];
    pieces[BLACK][5] = bitboards[Board::BLACK_KING];

    uint64_t occ_all = board.occupancy()[Board::OCC_BOTH];
    uint64_t from_bb = bit(from);
    uint64_t to_bb = bit(to);

    int moving_side = moving_white ? WHITE : BLACK;
    int enemy_side = moving_side ^ 1;

    int moving_type = piece_type_from_char(moving_piece);
    if (moving_type < 0) return piece_value(captured_piece);

    pieces[moving_side][moving_type] &= ~from_bb;
    occ_all &= ~from_bb;

    if (en_passant) {
        int ep_sq = to + (moving_white ? -8 : 8);
        uint64_t ep_bb = bit(ep_sq);
        pieces[enemy_side][0] &= ~ep_bb;
        occ_all &= ~ep_bb;
    } else {
        int captured_type = piece_type_from_char(captured_piece);
        if (captured_type >= 0) {
            pieces[enemy_side][captured_type] &= ~to_bb;
        }
    }
    occ_all &= ~to_bb;

    int new_type = piece_type_from_char(promoted_piece);
    if (new_type < 0) new_type = moving_type;
    pieces[moving_side][new_type] |= to_bb;
    occ_all |= to_bb;

    std::array<int, 32> gains{};
    gains[0] = piece_value(captured_piece);
    int depth = 0;

    moving_side ^= 1;

    auto attackers = attackers_to(to, occ_all, pieces);

    while (true) {
        uint64_t attackers_bb = attackers[moving_side];
        if (!attackers_bb) break;

        int attacker_sq = -1;
        int attacker_type = -1;
        for (int type = 0; type < 6; ++type) {
            uint64_t bb = attackers_bb & pieces[moving_side][static_cast<size_t>(type)];
            if (bb) {
                attacker_sq = std::countr_zero(bb);
                attacker_type = type;
                break;
            }
        }

        if (attacker_type == -1) break;

        ++depth;
        gains[depth] = piece_value_from_type(attacker_type) - gains[depth - 1];
        if (std::max(-gains[depth - 1], gains[depth]) < 0) break;

        uint64_t attacker_bb = bit(attacker_sq);
        pieces[moving_side][attacker_type] &= ~attacker_bb;
        occ_all &= ~attacker_bb;

        for (int type = 0; type < 6; ++type) {
            if (pieces[moving_side ^ 1][static_cast<size_t>(type)] & to_bb) {
                pieces[moving_side ^ 1][static_cast<size_t>(type)] &= ~to_bb;
                break;
            }
        }
        occ_all &= ~to_bb;

        pieces[moving_side][attacker_type] |= to_bb;
        occ_all |= to_bb;

        attackers = attackers_to(to, occ_all, pieces);
        moving_side ^= 1;
    }

    while (depth > 0) {
        gains[depth - 1] = -std::max(-gains[depth - 1], gains[depth]);
        --depth;
    }

    return gains[0];
}

} // namespace engine::eval

