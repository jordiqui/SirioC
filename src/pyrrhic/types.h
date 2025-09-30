#pragma once

#include <optional>
#include <string>

namespace sirio::pyrrhic {

enum class Color { White, Black };

enum class PieceType { Pawn, Knight, Bishop, Rook, Queen, King };

struct Piece {
    PieceType type;
    Color color;
};

char piece_to_char(const Piece& piece);
std::optional<Piece> piece_from_char(char symbol);
std::string color_to_string(Color color);
Color opposite(Color color);

}  // namespace sirio::pyrrhic
