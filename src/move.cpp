#include "sirio/move.hpp"

#include <cctype>
#include <cstdlib>
#include <stdexcept>

#include "sirio/bitboard.hpp"
#include "sirio/board.hpp"
#include "sirio/movegen.hpp"

namespace sirio {

namespace {
char promotion_char(PieceType piece) {
    switch (piece) {
        case PieceType::Queen:
            return 'q';
        case PieceType::Rook:
            return 'r';
        case PieceType::Bishop:
            return 'b';
        case PieceType::Knight:
            return 'n';
        default:
            throw std::invalid_argument("Invalid promotion piece");
    }
}

PieceType piece_from_promotion_char(char symbol) {
    switch (symbol) {
        case 'q':
            return PieceType::Queen;
        case 'r':
            return PieceType::Rook;
        case 'b':
            return PieceType::Bishop;
        case 'n':
            return PieceType::Knight;
        default:
            throw std::invalid_argument("Unknown promotion piece");
    }
}

int square_from_uci(const std::string &text, std::size_t offset) {
    if (offset + 1 >= text.size()) {
        throw std::invalid_argument("Invalid UCI move format");
    }
    char file = text[offset];
    char rank = text[offset + 1];
    if (file < 'a' || file > 'h' || rank < '1' || rank > '8') {
        throw std::invalid_argument("Invalid square in UCI move");
    }
    return (rank - '1') * 8 + (file - 'a');
}

}  // namespace

std::string move_to_uci(const Move &move) {
    std::string result;
    result.reserve(5);
    result.push_back(static_cast<char>('a' + file_of(move.from)));
    result.push_back(static_cast<char>('1' + rank_of(move.from)));
    result.push_back(static_cast<char>('a' + file_of(move.to)));
    result.push_back(static_cast<char>('1' + rank_of(move.to)));
    if (move.promotion) {
        result.push_back(promotion_char(*move.promotion));
    }
    return result;
}

Move move_from_uci(const Board &board, const std::string &uci) {
    if (uci.size() < 4) {
        throw std::invalid_argument("UCI move too short");
    }
    std::string normalized = uci;
    if (normalized.size() == 5) {
        const unsigned char promotion_char_raw = static_cast<unsigned char>(normalized[4]);
        normalized[4] = static_cast<char>(std::tolower(promotion_char_raw));
    }

    auto legal_moves = generate_legal_moves(board);
    for (const Move &move : legal_moves) {
        if (move_to_uci(move) == normalized) {
            return move;
        }
    }

    throw std::invalid_argument("UCI move is not legal in the current position");
}

}  // namespace sirio

