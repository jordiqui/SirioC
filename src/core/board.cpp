#include "engine/core/board.hpp"
#include "engine/core/fen.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <vector>

namespace engine {

Board::Board() { set_startpos(); }

void Board::set_startpos() {
    set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

bool Board::set_fen(const std::string& fen) {
    if (!fen::is_valid_fen(fen)) return false;
    std::istringstream iss(fen);
    std::vector<std::string> tokens;
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);
    if (tokens.size() < 4) return false;

    std::array<char, 64> new_squares;
    std::fill(new_squares.begin(), new_squares.end(), '.');
    bool new_stm_white = tokens[1] == "w";
    uint8_t new_castling = 0;
    int new_en_passant = -1;

    int rank = 7;
    int file = 0;
    for (char c : tokens[0]) {
        if (c == '/') {
            if (file != 8) return false;
            --rank;
            if (rank < 0) return false;
            file = 0;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            file += c - '0';
        } else {
            if (file >= 8 || rank < 0) return false;
            new_squares[rank * 8 + file] = c;
            ++file;
        }
    }
    if (rank != 0 || file != 8) {
        return false;
    }

    if (tokens[2] != "-") {
        for (char c : tokens[2]) {
            switch (c) {
                case 'K': new_castling |= CASTLE_WHITE_K; break;
                case 'Q': new_castling |= CASTLE_WHITE_Q; break;
                case 'k': new_castling |= CASTLE_BLACK_K; break;
                case 'q': new_castling |= CASTLE_BLACK_Q; break;
                default: break;
            }
        }
    }

    if (tokens[3] != "-") {
        if (tokens[3].size() == 2) {
            char file_char = tokens[3][0];
            char rank_char = tokens[3][1];
            if (file_char >= 'a' && file_char <= 'h' && rank_char >= '1' && rank_char <= '8') {
                new_en_passant = (rank_char - '1') * 8 + (file_char - 'a');
            }
        }
    }

    squares_ = new_squares;
    stm_white_ = new_stm_white;
    castling_rights_ = new_castling;
    en_passant_square_ = new_en_passant;
    last_fen_ = fen;
    return true;
}

bool Board::apply_moves_uci(const std::vector<std::string>& uci_moves) {
    for (const auto& uci : uci_moves) {
        if (!make_move_uci(uci)) return false;
    }
    return true;
}

bool Board::make_move(Move move) {
    auto legal = generate_legal_moves();
    for (Move m : legal) {
        if (m == move) {
            do_move(m);
            return true;
        }
    }
    return false;
}

bool Board::make_move_uci(const std::string& uci) {
    auto legal = generate_legal_moves();
    for (Move m : legal) {
        if (move_to_uci(m) == uci) {
            do_move(m);
            return true;
        }
    }
    return false;
}

std::string Board::move_to_uci(Move move) const {
    if (move == MOVE_NONE) return "0000";
    std::string out;
    out.reserve(5);
    out += square_to_string(move_from(move));
    out += square_to_string(move_to(move));
    int promo = move_promo(move);
    if (promo) {
        static const char promo_chars[] = {' ', 'n', 'b', 'r', 'q'};
        if (promo >= 0 && promo < static_cast<int>(sizeof(promo_chars) / sizeof(promo_chars[0]))) {
            out.push_back(promo_chars[promo]);
        }
    }
    return out;
}

bool Board::is_white_piece(char piece) {
    return piece >= 'A' && piece <= 'Z';
}

bool Board::is_black_piece(char piece) {
    return piece >= 'a' && piece <= 'z';
}

bool Board::is_empty(char piece) {
    return piece == '.';
}

int Board::file_of(int sq) {
    return sq % 8;
}

int Board::rank_of(int sq) {
    return sq / 8;
}

int Board::to_index(int file, int rank) {
    return rank * 8 + file;
}

std::string Board::square_to_string(int sq) const {
    std::string s;
    s.push_back(static_cast<char>('a' + file_of(sq)));
    s.push_back(static_cast<char>('1' + rank_of(sq)));
    return s;
}

char Board::promotion_from_code(int code, bool white) const {
    switch (code) {
        case 1: return white ? 'N' : 'n';
        case 2: return white ? 'B' : 'b';
        case 3: return white ? 'R' : 'r';
        case 4: return white ? 'Q' : 'q';
        default: return white ? 'P' : 'p';
    }
}

} // namespace engine
