#include "engine/core/fen.hpp"
#include "engine/core/board.hpp"
#include <cctype>
#include <charconv>
#include <sstream>
#include <string>
#include <vector>

namespace engine { namespace fen {

static int piece_index_from_char(char c) {
    switch (c) {
    case 'P': return Board::WHITE_PAWN;
    case 'N': return Board::WHITE_KNIGHT;
    case 'B': return Board::WHITE_BISHOP;
    case 'R': return Board::WHITE_ROOK;
    case 'Q': return Board::WHITE_QUEEN;
    case 'K': return Board::WHITE_KING;
    case 'p': return Board::BLACK_PAWN;
    case 'n': return Board::BLACK_KNIGHT;
    case 'b': return Board::BLACK_BISHOP;
    case 'r': return Board::BLACK_ROOK;
    case 'q': return Board::BLACK_QUEEN;
    case 'k': return Board::BLACK_KING;
    default: return -1;
    }
}

static inline std::vector<std::string> split(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> out; std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

static bool parse_int(const std::string& token, int min_value) {
    int value = 0;
    auto first = token.data();
    auto last = token.data() + token.size();
    auto res = std::from_chars(first, last, value);
    if (res.ec != std::errc() || res.ptr != last) return false;
    return value >= min_value;
}

static bool validate_board_field(const std::string& board_field) {
    int rank = 7;
    int file = 0;
    for (char c : board_field) {
        if (c == '/') {
            if (file != 8 || rank == 0) return false;
            --rank;
            file = 0;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            int skip = c - '0';
            if (skip < 1 || skip > 8 || file + skip > 8) return false;
            file += skip;
            continue;
        }
        if (piece_index_from_char(c) == -1 || file >= 8) return false;
        ++file;
    }
    return rank == 0 && file == 8;
}

static bool validate_castling(const std::string& field) {
    if (field == "-") return true;
    uint8_t rights = 0;
    for (char c : field) {
        uint8_t mask = 0;
        switch (c) {
        case 'K': mask = Board::CASTLE_WHITE_KINGSIDE; break;
        case 'Q': mask = Board::CASTLE_WHITE_QUEENSIDE; break;
        case 'k': mask = Board::CASTLE_BLACK_KINGSIDE; break;
        case 'q': mask = Board::CASTLE_BLACK_QUEENSIDE; break;
        default: return false;
        }
        if (rights & mask) return false;
        rights |= mask;
    }
    return true;
}

static bool validate_en_passant(const std::string& field) {
    if (field == "-") return true;
    if (field.size() != 2) return false;
    char file = field[0];
    char rank = field[1];
    if (file < 'a' || file > 'h') return false;
    if (rank < '1' || rank > '8') return false;
    return true;
}

bool is_valid_fen(const std::string& fen) {
    auto t = split(fen);
    if (t.size() != 6) return false;

    if (!validate_board_field(t[0])) return false;
    if (t[1] != "w" && t[1] != "b") return false;
    if (!validate_castling(t[2])) return false;
    if (!validate_en_passant(t[3])) return false;
    if (!parse_int(t[4], 0)) return false;
    if (!parse_int(t[5], 1)) return false;
    return true;
}

}} // namespace
