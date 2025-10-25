#include "sirio/endgame.hpp"

#include <bit>

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

}  // namespace sirio

