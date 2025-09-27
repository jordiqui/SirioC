#include "engine/core/board.hpp"
#include "engine/core/fen.hpp"
#include <array>
#include <cctype>
#include <charconv>
#include <sstream>

namespace engine {

namespace {

constexpr uint64_t kSquareMask(int square) { return 1ULL << square; }

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

void Board::set_startpos() {
    (void)set_fen(std::string(kStartposFEN));
}

bool Board::set_fen(const std::string& fen) {
    if (!fen::is_valid_fen(fen)) return false;
    std::istringstream iss(fen);
    std::string board_field, stm_field, castling_field, ep_field, halfmove_field, fullmove_field;
    if (!(iss >> board_field >> stm_field >> castling_field >> ep_field >> halfmove_field >> fullmove_field)) {
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
    board_ = board_array;
    castling_rights_ = castling;
    en_passant_square_ = ep_square;
    halfmove_clock_ = halfmove;
    fullmove_number_ = fullmove;
    stm_white_ = stm_white;
    last_fen_ = fen;
    return true;
}

bool Board::apply_moves_uci(const std::vector<std::string>& /*uci_moves*/) {
    // TODO: translate UCI strings to Move, call make_move
    return true;
}

} // namespace engine
