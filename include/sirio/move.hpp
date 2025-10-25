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

}  // namespace sirio

