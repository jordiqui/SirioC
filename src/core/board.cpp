#include "engine/core/board.hpp"
#include "engine/core/fen.hpp"
#include "engine/eval/nnue/accumulator.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <random>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace engine {

namespace {

constexpr uint64_t kSquareMask(int square) { return 1ULL << square; }

const std::array<uint64_t, 12 * 64>& piece_keys() {
    static std::array<uint64_t, 12 * 64> keys{};
    static bool initialized = false;
    if (!initialized) {
        std::mt19937_64 rng(0xC0FFEE123456789ULL);
        for (auto& k : keys) k = rng();
        initialized = true;
    }
    return keys;
}

const std::array<uint64_t, 16>& castling_keys() {
    static std::array<uint64_t, 16> keys{};
    static bool initialized = false;
    if (!initialized) {
        std::mt19937_64 rng(0xABCDEF9876543210ULL);
        for (auto& k : keys) k = rng();
        initialized = true;
    }
    return keys;
}

const std::array<uint64_t, 8>& enpassant_keys() {
    static std::array<uint64_t, 8> keys{};
    static bool initialized = false;
    if (!initialized) {
        std::mt19937_64 rng(0x13579BDF2468ACE0ULL);
        for (auto& k : keys) k = rng();
        initialized = true;
    }
    return keys;
}

uint64_t side_key() {
    static uint64_t key = 0;
    static bool initialized = false;
    if (!initialized) {
        std::mt19937_64 rng(0x1122334455667788ULL);
        key = rng();
        initialized = true;
    }
    return key;
}

int piece_index_from_char(char c) {
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

bool parse_int(const std::string& token, int min_value, int& out) {
    int value = 0;
    auto first = token.data();
    auto last = token.data() + token.size();
    auto res = std::from_chars(first, last, value);
    if (res.ec != std::errc() || res.ptr != last) return false;
    if (value < min_value) return false;
    out = value;
    return true;
}

int square_from_algebraic(const std::string& alg) {
    if (alg.size() != 2) return Board::INVALID_SQUARE;
    char file = alg[0];
    char rank = alg[1];
    if (file < 'a' || file > 'h') return Board::INVALID_SQUARE;
    if (rank < '1' || rank > '8') return Board::INVALID_SQUARE;
    int file_idx = file - 'a';
    int rank_idx = rank - '1';
    return rank_idx * 8 + file_idx;
}

bool parse_board_field(const std::string& field,
                       std::array<uint64_t, Board::PIECE_NB>& piece_bb,
                       std::array<uint64_t, Board::OCC_NB>& occupancy,
                       std::array<char, 64>& board_array) {
    piece_bb.fill(0ULL);
    occupancy.fill(0ULL);
    board_array.fill('.');

    int rank = 7;
    int file = 0;
    uint64_t white_occ = 0ULL;
    uint64_t black_occ = 0ULL;

    for (char c : field) {
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

        int piece_index = piece_index_from_char(c);
        if (piece_index == -1 || file >= 8) return false;

        int square = rank * 8 + file;
        uint64_t mask = kSquareMask(square);
        piece_bb[static_cast<size_t>(piece_index)] |= mask;
        if (piece_index <= Board::WHITE_KING) {
            white_occ |= mask;
        } else {
            black_occ |= mask;
        }
        board_array[static_cast<size_t>(square)] = c;
        ++file;
    }

    if (rank != 0 || file != 8) return false;

    occupancy[Board::OCC_WHITE] = white_occ;
    occupancy[Board::OCC_BLACK] = black_occ;
    occupancy[Board::OCC_BOTH] = white_occ | black_occ;
    return true;
}

bool parse_castling_field(const std::string& field, uint8_t& rights) {
    rights = 0;
    if (field == "-") return true;
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

bool parse_en_passant_field(const std::string& field, int& square) {
    if (field == "-") {
        square = Board::INVALID_SQUARE;
        return true;
    }
    square = square_from_algebraic(field);
    if (square == Board::INVALID_SQUARE) return false;
    return true;
}

} // namespace

Board::Board() { set_startpos(); }

void Board::set_startpos() { set_fen(std::string(kStartposFEN)); }

bool Board::set_fen(const std::string& fen) {
    if (!fen::is_valid_fen(fen)) return false;

    std::istringstream iss(fen);
    std::string board_field, stm_field, castling_field, ep_field, halfmove_field,
        fullmove_field;
    if (!(iss >> board_field >> stm_field >> castling_field >> ep_field >> halfmove_field >>
          fullmove_field)) {
        return false;
    }
    std::string extra;
    if (iss >> extra) return false;

    std::array<uint64_t, PIECE_NB> piece_bb{};
    std::array<uint64_t, OCC_NB> occupancy{};
    std::array<char, 64> board_array{};
    uint8_t castling = 0;
    int ep_square = INVALID_SQUARE;
    int halfmove = 0;
    int fullmove = 1;
    bool stm_white = false;

    if (!parse_board_field(board_field, piece_bb, occupancy, board_array)) return false;

    if (stm_field == "w") stm_white = true;
    else if (stm_field == "b") stm_white = false;
    else return false;

    if (!parse_castling_field(castling_field, castling)) return false;
    if (!parse_en_passant_field(ep_field, ep_square)) return false;
    if (!parse_int(halfmove_field, 0, halfmove)) return false;
    if (!parse_int(fullmove_field, 1, fullmove)) return false;

    piece_bitboards_ = piece_bb;
    occupancy_ = occupancy;
    squares_ = board_array;
    castling_rights_ = castling;
    en_passant_square_ = ep_square;
    halfmove_clock_ = halfmove;
    fullmove_number_ = fullmove;
    stm_white_ = stm_white;
    last_fen_ = fen;
    history_.clear();
    accumulator_.reset(*this);
    return true;
}

bool Board::apply_moves_uci(const std::vector<std::string>& uci_moves) {
    for (const auto& uci : uci_moves) {
        if (!make_move_uci(uci)) return false;
    }
    return true;
}

bool Board::make_move(Move m) {
    auto legal = generate_legal_moves();
    for (Move move : legal) {
        if (move == m) {
            State state;
            apply_move(move, state);
            history_.push_back(state);
            return true;
        }
    }
    return false;
}

bool Board::make_move_uci(const std::string& uci) {
    auto legal = generate_legal_moves();
    for (Move move : legal) {
        if (move_to_uci(move) == uci) {
            State state;
            apply_move(move, state);
            history_.push_back(state);
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

Board Board::after_move(Move move) const {
    Board copy(*this);
    copy.history_.clear();
    State state;
    copy.apply_move(move, state);
    copy.history_.push_back(state);
    return copy;
}

bool Board::is_white_piece(char piece) { return piece >= 'A' && piece <= 'Z'; }

bool Board::is_black_piece(char piece) { return piece >= 'a' && piece <= 'z'; }

bool Board::is_empty(char piece) { return piece == '.'; }

int Board::file_of(int sq) { return sq % 8; }

int Board::rank_of(int sq) { return sq / 8; }

int Board::to_index(int file, int rank) { return rank * 8 + file; }

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

uint64_t Board::zobrist_key() const {
    uint64_t key = 0;
    const auto& pk = piece_keys();
    for (int sq = 0; sq < 64; ++sq) {
        char piece = squares_[sq];
        int idx = piece_index_from_char(piece);
        if (idx == -1) continue;
        key ^= pk[static_cast<size_t>(idx) * 64 + static_cast<size_t>(sq)];
    }
    if (stm_white_) key ^= side_key();
    key ^= castling_keys()[castling_rights_ & 0xF];
    if (en_passant_square_ != INVALID_SQUARE) {
        key ^= enpassant_keys()[file_of(en_passant_square_)];
    }
    return key;
}

} // namespace engine
