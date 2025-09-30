#include "files/fen.h"

#include "pyrrhic/types.h"

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace sirio::files {

namespace {

void parse_piece_placement(const std::string& placement, pyrrhic::Board& board) {
    int rank = 7;
    int file = 0;
    for (char symbol : placement) {
        if (symbol == '/') {
            --rank;
            file = 0;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(symbol))) {
            file += symbol - '0';
            continue;
        }

        const auto piece = pyrrhic::piece_from_char(symbol);
        if (!piece.has_value()) {
            throw std::runtime_error("Invalid piece symbol in FEN");
        }
        const int square = pyrrhic::make_square(file, rank);
        board.set_piece(square, piece);
        ++file;
    }
}

std::optional<int> parse_en_passant(const std::string& token) {
    if (token == "-" || token.empty()) {
        return std::nullopt;
    }

    if (token.size() != 2) {
        throw std::runtime_error("Invalid en passant square in FEN");
    }

    const int file = token[0] - 'a';
    const int rank = token[1] - '1';
    if (file < 0 || file > 7 || rank < 0 || rank > 7) {
        throw std::runtime_error("Invalid en passant coordinates");
    }

    return pyrrhic::make_square(file, rank);
}

}  // namespace

pyrrhic::Board parse_fen(const std::string& fen) {
    std::istringstream stream(fen);
    std::vector<std::string> tokens;
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }

    if (tokens.size() < 4) {
        throw std::runtime_error("FEN string must contain at least four fields");
    }

    pyrrhic::Board board;
    board.clear();
    parse_piece_placement(tokens[0], board);

    board.set_side_to_move(tokens[1] == "b" ? pyrrhic::Color::Black : pyrrhic::Color::White);
    board.set_castling_rights(tokens[2]);
    board.set_en_passant_square(parse_en_passant(tokens[3]));

    if (tokens.size() >= 5) {
        board.set_halfmove_clock(std::stoi(tokens[4]));
    }
    if (tokens.size() >= 6) {
        board.set_fullmove_number(std::stoi(tokens[5]));
    }

    return board;
}

std::string to_fen(const pyrrhic::Board& board) {
    std::ostringstream fen;
    for (int rank = 7; rank >= 0; --rank) {
        int empty_count = 0;
        for (int file = 0; file < 8; ++file) {
            const int square = pyrrhic::make_square(file, rank);
            const auto piece = board.piece_at(square);
            if (piece.has_value()) {
                if (empty_count != 0) {
                    fen << empty_count;
                    empty_count = 0;
                }
                fen << pyrrhic::piece_to_char(*piece);
            } else {
                ++empty_count;
            }
        }
        if (empty_count != 0) {
            fen << empty_count;
        }
        if (rank > 0) {
            fen << '/';
        }
    }

    fen << ' ' << (board.side_to_move() == pyrrhic::Color::White ? 'w' : 'b');
    fen << ' ' << board.castling_rights();

    if (const auto ep = board.en_passant_square(); ep.has_value()) {
        fen << ' ' << static_cast<char>('a' + pyrrhic::file_of(*ep))
            << static_cast<char>('1' + pyrrhic::rank_of(*ep));
    } else {
        fen << " -";
    }

    fen << ' ' << board.halfmove_clock();
    fen << ' ' << board.fullmove_number();

    return fen.str();
}

}  // namespace sirio::files
