#include "sirio/endgame.hpp"

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <optional>

#include "sirio/bitboard.hpp"

namespace sirio {

namespace {

bool has_major_piece_or_pawn(const Board &board, Color color) {
    return board.pieces(color, PieceType::Queen) != 0 ||
           board.pieces(color, PieceType::Rook) != 0 ||
           board.pieces(color, PieceType::Pawn) != 0;
}

bool has_bishop_and_knight(const Board &board, Color color) {
    return board.pieces(color, PieceType::Bishop) != 0 &&
           board.pieces(color, PieceType::Knight) != 0;
}

bool has_three_knights(const Board &board, Color color) {
    return std::popcount(board.pieces(color, PieceType::Knight)) >= 3;
}

bool has_additional_material(const Board &board, Color color) {
    for (PieceType type : {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight}) {
        if (board.pieces(color, type) != 0) {
            return true;
        }
    }
    return false;
}

int king_distance(int from, int to) {
    if (from < 0 || to < 0) {
        return 8;
    }
    int file_diff = std::abs(file_of(from) - file_of(to));
    int rank_diff = std::abs(rank_of(from) - rank_of(to));
    return std::max(file_diff, rank_diff);
}

std::optional<int> evaluate_single_pawn_vs_king(const Board &board, Color strong) {
    const Color weak = opposite(strong);
    if (has_additional_material(board, strong) || has_additional_material(board, weak)) {
        return std::nullopt;
    }

    Bitboard strong_pawns = board.pieces(strong, PieceType::Pawn);
    Bitboard weak_pawns = board.pieces(weak, PieceType::Pawn);
    if (std::popcount(strong_pawns) != 1 || weak_pawns != 0) {
        return std::nullopt;
    }

    int pawn_square = bit_scan_forward(strong_pawns);
    int strong_king = board.king_square(strong);
    int weak_king = board.king_square(weak);
    int file = file_of(pawn_square);
    int rank = rank_of(pawn_square);
    int promotion_rank = strong == Color::White ? 7 : 0;
    int target_square = promotion_rank * 8 + file;

    int strong_distance = king_distance(strong_king, target_square);
    int weak_distance = king_distance(weak_king, target_square);

    int advancement = strong == Color::White ? rank : (7 - rank);
    int score = 120 * advancement;

    bool defender_in_front = false;
    if (strong == Color::White) {
        defender_in_front = rank_of(weak_king) >= rank && std::abs(file_of(weak_king) - file) <= 1;
    } else {
        defender_in_front = rank_of(weak_king) <= rank && std::abs(file_of(weak_king) - file) <= 1;
    }

    if (strong_distance + 1 < weak_distance) {
        score += 500;
    } else if (weak_distance <= strong_distance) {
        score -= 400;
    }

    if (defender_in_front && weak_distance <= strong_distance + 1) {
        score -= 250;
    }

    if (file == 0 || file == 7) {
        // Rook pawn specific heuristics
        int corner_square = strong == Color::White ? (file == 0 ? 56 : 63) : (file == 0 ? 0 : 7);
        if (weak_king == corner_square) {
            score -= 300;
        }
    }

    if (strong == Color::Black) {
        score = -score;
    }

    return score;
}

}  // namespace

bool sufficient_material_to_force_checkmate(const Board &board) {
    if (has_major_piece_or_pawn(board, Color::White) ||
        has_major_piece_or_pawn(board, Color::Black)) {
        return true;
    }

    if (board.has_bishop_pair(Color::White) || board.has_bishop_pair(Color::Black)) {
        return true;
    }

    if (has_bishop_and_knight(board, Color::White) ||
        has_bishop_and_knight(board, Color::Black)) {
        return true;
    }

    if (has_three_knights(board, Color::White) || has_three_knights(board, Color::Black)) {
        return true;
    }

    return false;
}

std::optional<int> evaluate_specialized_endgame(const Board &board) {
    if (auto eval = evaluate_single_pawn_vs_king(board, Color::White); eval.has_value()) {
        return eval;
    }
    if (auto eval = evaluate_single_pawn_vs_king(board, Color::Black); eval.has_value()) {
        return eval;
    }
    return std::nullopt;
}

}  // namespace sirio

