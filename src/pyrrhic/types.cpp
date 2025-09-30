#include "pyrrhic/types.h"

#include <array>
#include <cctype>
#include <stdexcept>

namespace sirio::pyrrhic {

namespace {

constexpr std::array<char, 6> WHITE_PIECES = {'P', 'N', 'B', 'R', 'Q', 'K'};
constexpr std::array<char, 6> BLACK_PIECES = {'p', 'n', 'b', 'r', 'q', 'k'};

char encode(PieceType type, Color color) {
    const auto& table = color == Color::White ? WHITE_PIECES : BLACK_PIECES;
    return table.at(static_cast<std::size_t>(type));
}

PieceType decode_piece_type(char symbol) {
    switch (static_cast<char>(std::tolower(symbol))) {
        case 'p':
            return PieceType::Pawn;
        case 'n':
            return PieceType::Knight;
        case 'b':
            return PieceType::Bishop;
        case 'r':
            return PieceType::Rook;
        case 'q':
            return PieceType::Queen;
        case 'k':
            return PieceType::King;
        default:
            throw std::invalid_argument("Invalid piece symbol");
    }
}

Color decode_piece_color(char symbol) {
    return std::isupper(static_cast<unsigned char>(symbol)) ? Color::White : Color::Black;
}

}  // namespace

char piece_to_char(const Piece& piece) {
    return encode(piece.type, piece.color);
}

std::optional<Piece> piece_from_char(char symbol) {
    if (symbol == '-' || symbol == '.' || symbol == '0') {
        return std::nullopt;
    }

    if (!std::isalpha(static_cast<unsigned char>(symbol))) {
        return std::nullopt;
    }

    try {
        return Piece{decode_piece_type(symbol), decode_piece_color(symbol)};
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }
}

std::string color_to_string(Color color) {
    return color == Color::White ? "white" : "black";
}

Color opposite(Color color) {
    return color == Color::White ? Color::Black : Color::White;
}

}  // namespace sirio::pyrrhic
