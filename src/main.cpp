#include <iostream>
#include <sstream>
#include <string>

#include "sirio/bitboard.hpp"
#include "sirio/board.hpp"

namespace {
std::string join_arguments(int argc, char **argv) {
    if (argc <= 1) {
        return {};
    }
    std::ostringstream builder;
    for (int i = 1; i < argc; ++i) {
        if (i > 1) {
            builder << ' ';
        }
        builder << argv[i];
    }
    return builder.str();
}

char piece_to_char(sirio::Color color, sirio::PieceType type) {
    switch (type) {
        case sirio::PieceType::Pawn:
            return color == sirio::Color::White ? 'P' : 'p';
        case sirio::PieceType::Knight:
            return color == sirio::Color::White ? 'N' : 'n';
        case sirio::PieceType::Bishop:
            return color == sirio::Color::White ? 'B' : 'b';
        case sirio::PieceType::Rook:
            return color == sirio::Color::White ? 'R' : 'r';
        case sirio::PieceType::Queen:
            return color == sirio::Color::White ? 'Q' : 'q';
        case sirio::PieceType::King:
            return color == sirio::Color::White ? 'K' : 'k';
        case sirio::PieceType::Count:
            break;
    }
    return '?';
}

char symbol_on_square(const sirio::Board &board, int square) {
    const sirio::Bitboard mask = sirio::one_bit(square);
    for (std::size_t index = 0; index < static_cast<std::size_t>(sirio::PieceType::Count); ++index) {
        const auto type = static_cast<sirio::PieceType>(index);
        if (board.pieces(sirio::Color::White, type) & mask) {
            return piece_to_char(sirio::Color::White, type);
        }
        if (board.pieces(sirio::Color::Black, type) & mask) {
            return piece_to_char(sirio::Color::Black, type);
        }
    }
    return '.';
}

void print_board(const sirio::Board &board) {
    std::cout << "  +------------------------+\n";
    for (int rank = 7; rank >= 0; --rank) {
        std::cout << rank + 1 << " |";
        for (int file = 0; file < 8; ++file) {
            const int square = rank * 8 + file;
            std::cout << ' ' << symbol_on_square(board, square);
        }
        std::cout << " |\n";
    }
    std::cout << "  +------------------------+\n";
    std::cout << "    a b c d e f g h\n";
}
}

int main(int argc, char **argv) {
    try {
        const std::string fen = join_arguments(argc, argv);
        sirio::Board board = fen.empty() ? sirio::Board{} : sirio::Board{fen};
        print_board(board);
        std::cout << "FEN: " << board.to_fen() << "\n";
    } catch (const std::exception &ex) {
        std::cerr << "Failed to initialise board: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}

