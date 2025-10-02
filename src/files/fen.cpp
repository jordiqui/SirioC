#include "files/fen.h"

#include "pyrrhic/types.h"

#include <cctype>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace sirio::files {

namespace {

bool parse_piece_placement(const std::string& placement, pyrrhic::Board& board,
                           std::string& error) {
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
            error = "Invalid piece symbol in FEN";
            return false;
        }

        if (file < 0 || file > 7 || rank < 0 || rank > 7) {
            error = "Invalid piece placement coordinates";
            return false;
        }

        const int square = pyrrhic::make_square(file, rank);
        board.set_piece(square, piece);
        ++file;
    }

    return true;
}

bool parse_en_passant(const std::string& token, std::optional<int>& square, std::string& error) {
    if (token == "-" || token.empty()) {
        square.reset();
        return true;
    }

    if (token.size() != 2) {
        error = "Invalid en passant square in FEN";
        return false;
    }

    const int file = token[0] - 'a';
    const int rank = token[1] - '1';
    if (file < 0 || file > 7 || rank < 0 || rank > 7) {
        error = "Invalid en passant coordinates";
        return false;
    }

    square = pyrrhic::make_square(file, rank);
    return true;
}

bool parse_integer_field(const std::string& token, int& value, std::string& error) {
    if (token.empty()) {
        error = "Empty integer field in FEN";
        return false;
    }

    int result = 0;
    const char* begin = token.data();
    const char* end = begin + token.size();
    const auto parsed = std::from_chars(begin, end, result);
    if (parsed.ec != std::errc{} || parsed.ptr != end) {
        error = "Invalid integer field in FEN";
        return false;
    }

    value = result;
    return true;
}

void assign_error(std::string* output, const std::string& error) {
    if (output) {
        *output = error;
    }
}

}  // namespace

std::optional<pyrrhic::Board> try_parse_fen(const std::string& fen, std::string* error_message) {
    std::istringstream stream(fen);
    std::vector<std::string> tokens;
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }

    if (tokens.size() < 4) {
        assign_error(error_message, "FEN string must contain at least four fields");
        return std::nullopt;
    }

    pyrrhic::Board board;
    board.clear();

    std::string error;
    if (!parse_piece_placement(tokens[0], board, error)) {
        assign_error(error_message, error);
        return std::nullopt;
    }

    if (tokens[1] == "b") {
        board.set_side_to_move(pyrrhic::Color::Black);
    } else if (tokens[1] == "w") {
        board.set_side_to_move(pyrrhic::Color::White);
    } else {
        assign_error(error_message, "Invalid side to move in FEN");
        return std::nullopt;
    }

    board.set_castling_rights(tokens[2]);

    std::optional<int> en_passant_square;
    if (!parse_en_passant(tokens[3], en_passant_square, error)) {
        assign_error(error_message, error);
        return std::nullopt;
    }
    board.set_en_passant_square(en_passant_square);

    if (tokens.size() >= 5) {
        int halfmove = 0;
        if (!parse_integer_field(tokens[4], halfmove, error)) {
            assign_error(error_message, error);
            return std::nullopt;
        }
        board.set_halfmove_clock(halfmove);
    }

    if (tokens.size() >= 6) {
        int fullmove = 0;
        if (!parse_integer_field(tokens[5], fullmove, error)) {
            assign_error(error_message, error);
            return std::nullopt;
        }
        board.set_fullmove_number(fullmove);
    }

    return board;
}

pyrrhic::Board parse_fen(const std::string& fen) {
    std::string error;
    auto board = try_parse_fen(fen, &error);
    if (board.has_value()) {
        return *board;
    }

#if defined(__cpp_exceptions)
    throw std::runtime_error(error);
#else
    std::fprintf(stderr, "FEN parse error: %s\n", error.c_str());
    std::abort();
#endif
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
