#pragma once

#include <optional>
#include <string>

#include "sirio/board.hpp"

namespace sirio {

struct Move {
    int from = 0;
    int to = 0;
    PieceType piece = PieceType::Pawn;
    std::optional<PieceType> captured;
    std::optional<PieceType> promotion;
    bool is_en_passant = false;
    bool is_castling = false;

    Move() = default;
    Move(int from_square, int to_square, PieceType moving_piece)
        : from(from_square), to(to_square), piece(moving_piece) {}
};

std::string move_to_uci(const Move &move);
Move move_from_uci(const Board &board, const std::string &uci);
bool validate_move(const Board &board, const Move &move, Board *next_board = nullptr);

// Applies a move expressed in UCI format to the given board.
// Returns true if the token was parsed and applied successfully. This helper
// understands the special "0000" token used by the UCI protocol to denote a
// null move and will call Board::apply_null_move in that case. When the token
// is not recognised or represents an illegal move for the current position the
// function leaves the board unchanged and returns false.
bool apply_uci_move(Board &board, const std::string &uci_token);

}  // namespace sirio

