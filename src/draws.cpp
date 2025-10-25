#include "sirio/draws.hpp"

#include <bit>

namespace sirio {
namespace {

bool is_light_square(int square) {
    return ((file_of(square) + rank_of(square)) & 1) != 0;
}

}  // namespace

bool draw_by_fifty_move_rule(const Board &board) {
    return board.halfmove_clock() >= fifty_move_rule_limit;
}

int draw_by_repetition_rule(const Board &board) {
    const auto &history = board.history();
    if (history.empty()) {
        return 0;
    }

    const std::uint64_t target_hash = board.zobrist_hash();
    int count = 0;

    for (std::size_t index = history.size(); index-- > 0;) {
        const GameState &state = history.at(index);
        if (state.zobrist_hash == target_hash) {
            ++count;
        }
        if (state.halfmove_clock == 0) {
            break;
        }
    }

    return count;
}

bool draw_by_threefold_repetition(const Board &board) {
    return draw_by_repetition_rule(board) >= 3;
}

bool draw_by_insufficient_material_rule(const Board &board) {
    const Bitboard white_queens = board.pieces(Color::White, PieceType::Queen);
    const Bitboard white_rooks = board.pieces(Color::White, PieceType::Rook);
    const Bitboard white_pawns = board.pieces(Color::White, PieceType::Pawn);
    const Bitboard black_queens = board.pieces(Color::Black, PieceType::Queen);
    const Bitboard black_rooks = board.pieces(Color::Black, PieceType::Rook);
    const Bitboard black_pawns = board.pieces(Color::Black, PieceType::Pawn);

    const bool has_major_or_pawn = white_queens != 0 || white_rooks != 0 || white_pawns != 0 ||
                                   black_queens != 0 || black_rooks != 0 || black_pawns != 0;
    if (has_major_or_pawn) {
        return false;
    }

    const Bitboard white_bishops = board.pieces(Color::White, PieceType::Bishop);
    const Bitboard white_knights = board.pieces(Color::White, PieceType::Knight);
    const Bitboard black_bishops = board.pieces(Color::Black, PieceType::Bishop);
    const Bitboard black_knights = board.pieces(Color::Black, PieceType::Knight);

    const bool king_vs_king = white_bishops == 0 && white_knights == 0 && black_bishops == 0 &&
                              black_knights == 0;

    const bool kb_vs_k = std::popcount(white_bishops) == 1 && white_knights == 0 &&
                         black_bishops == 0 && black_knights == 0;
    const bool kn_vs_k = white_bishops == 0 && std::popcount(white_knights) == 1 &&
                         black_bishops == 0 && black_knights == 0;

    const bool k_vs_kb = white_bishops == 0 && white_knights == 0 && std::popcount(black_bishops) == 1 &&
                         black_knights == 0;
    const bool k_vs_kn = white_bishops == 0 && white_knights == 0 && black_bishops == 0 &&
                         std::popcount(black_knights) == 1;

    const bool kb_vs_kb = std::popcount(white_bishops) == 1 && white_knights == 0 &&
                          std::popcount(black_bishops) == 1 && black_knights == 0;

    bool same_color_bishops = false;
    if (kb_vs_kb) {
        const int white_square = std::countr_zero(white_bishops);
        const int black_square = std::countr_zero(black_bishops);
        same_color_bishops = is_light_square(white_square) == is_light_square(black_square);
    }

    if (king_vs_king || kb_vs_k || kn_vs_k || k_vs_kb || k_vs_kn || (kb_vs_kb && same_color_bishops)) {
        return true;
    }

    return false;
}

}  // namespace sirio

