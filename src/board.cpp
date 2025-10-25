#include "sirio/board.hpp"

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>

namespace sirio {

namespace {
constexpr const char *kStartPositionFEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

constexpr std::size_t piece_count = static_cast<std::size_t>(PieceType::Count);
}  // namespace

Board::Board() {
    set_from_fen(kStartPositionFEN);
}

Board::Board(std::string_view fen) {
    set_from_fen(fen);
}

void Board::clear() {
    white_.fill(0);
    black_.fill(0);
    occupancy_ = 0;
    side_to_move_ = Color::White;
    castling_ = {};
    halfmove_clock_ = 0;
    fullmove_number_ = 1;
    en_passant_square_ = -1;
}

Bitboard &Board::pieces_ref(Color color, PieceType type) {
    auto index = static_cast<std::size_t>(type);
    return color == Color::White ? white_[index] : black_[index];
}

const Bitboard &Board::pieces_ref(Color color, PieceType type) const {
    auto index = static_cast<std::size_t>(type);
    return color == Color::White ? white_[index] : black_[index];
}

Bitboard Board::pieces(Color color, PieceType type) const {
    return pieces_ref(color, type);
}

Bitboard Board::occupancy(Color color) const {
    const auto &source = color == Color::White ? white_ : black_;
    Bitboard occ = 0;
    for (Bitboard bb : source) {
        occ |= bb;
    }
    return occ;
}

std::optional<int> Board::en_passant_square() const {
    if (en_passant_square_ < 0) {
        return std::nullopt;
    }
    return en_passant_square_;
}

std::optional<std::pair<Color, PieceType>> Board::piece_at(int square) const {
    const Bitboard target = one_bit(square);
    for (std::size_t index = 0; index < piece_count; ++index) {
        if (white_[index] & target) {
            return std::make_pair(Color::White, static_cast<PieceType>(index));
        }
        if (black_[index] & target) {
            return std::make_pair(Color::Black, static_cast<PieceType>(index));
        }
    }
    return std::nullopt;
}

PieceType Board::piece_type_from_char(char piece) {
    switch (piece) {
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
            throw std::invalid_argument("Unknown piece type");
    }
}

char Board::piece_to_char(Color color, PieceType type) {
    switch (type) {
        case PieceType::Pawn:
            return color == Color::White ? 'P' : 'p';
        case PieceType::Knight:
            return color == Color::White ? 'N' : 'n';
        case PieceType::Bishop:
            return color == Color::White ? 'B' : 'b';
        case PieceType::Rook:
            return color == Color::White ? 'R' : 'r';
        case PieceType::Queen:
            return color == Color::White ? 'Q' : 'q';
        case PieceType::King:
            return color == Color::White ? 'K' : 'k';
        case PieceType::Count:
            break;
    }
    throw std::logic_error("Invalid piece type for conversion");
}

int Board::square_from_string(std::string_view square) {
    if (square.size() != 2) {
        return -1;
    }
    char file = square[0];
    char rank = square[1];
    if (file < 'a' || file > 'h' || rank < '1' || rank > '8') {
        return -1;
    }
    int file_index = file - 'a';
    int rank_index = rank - '1';
    return rank_index * 8 + file_index;
}

std::string Board::square_to_string(int square) {
    if (square < 0 || square >= 64) {
        return "-";
    }
    std::string result(2, ' ');
    result[0] = static_cast<char>('a' + file_of(square));
    result[1] = static_cast<char>('1' + rank_of(square));
    return result;
}

void Board::set_from_fen(std::string_view fen) {
    clear();
    std::string fen_text{fen};
    std::istringstream stream{fen_text};

    std::string placement;
    std::string active_color;
    std::string castling_rights_text;
    std::string en_passant_text;
    std::string halfmove_text;
    std::string fullmove_text;

    if (!(stream >> placement >> active_color >> castling_rights_text >> en_passant_text >> halfmove_text >> fullmove_text)) {
        throw std::invalid_argument("FEN string is missing required fields");
    }

    int rank = 7;
    int file = 0;
    for (char symbol : placement) {
        if (symbol == '/') {
            if (file != 8) {
                throw std::invalid_argument("Invalid FEN rank length");
            }
            --rank;
            file = 0;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(symbol))) {
            file += symbol - '0';
            if (file > 8) {
                throw std::invalid_argument("Too many squares in FEN rank");
            }
            continue;
        }

        if (!std::isalpha(static_cast<unsigned char>(symbol))) {
            throw std::invalid_argument("Unexpected character in FEN placement");
        }

        if (rank < 0 || file >= 8) {
            throw std::invalid_argument("FEN placement contains too many squares");
        }

        Color color = std::isupper(static_cast<unsigned char>(symbol)) ? Color::White : Color::Black;
        PieceType type = piece_type_from_char(static_cast<char>(std::tolower(static_cast<unsigned char>(symbol))));
        int square = rank * 8 + file;
        Bitboard mask = one_bit(square);
        pieces_ref(color, type) |= mask;
        occupancy_ |= mask;
        ++file;
    }

    if (rank != 0 || file != 8) {
        throw std::invalid_argument("FEN placement does not describe all squares");
    }

    if (active_color == "w") {
        side_to_move_ = Color::White;
    } else if (active_color == "b") {
        side_to_move_ = Color::Black;
    } else {
        throw std::invalid_argument("Invalid active color in FEN");
    }

    if (castling_rights_text == "-") {
        castling_ = {};
    } else {
        for (char c : castling_rights_text) {
            switch (c) {
                case 'K':
                    castling_.white_kingside = true;
                    break;
                case 'Q':
                    castling_.white_queenside = true;
                    break;
                case 'k':
                    castling_.black_kingside = true;
                    break;
                case 'q':
                    castling_.black_queenside = true;
                    break;
                default:
                    throw std::invalid_argument("Invalid castling rights in FEN");
            }
        }
    }

    if (en_passant_text == "-") {
        en_passant_square_ = -1;
    } else {
        en_passant_square_ = square_from_string(en_passant_text);
        if (en_passant_square_ < 0) {
            throw std::invalid_argument("Invalid en passant square in FEN");
        }
    }

    try {
        halfmove_clock_ = std::stoi(halfmove_text);
        fullmove_number_ = std::stoi(fullmove_text);
    } catch (const std::exception &) {
        throw std::invalid_argument("Invalid move counters in FEN");
    }

    if (halfmove_clock_ < 0 || fullmove_number_ <= 0) {
        throw std::invalid_argument("FEN counters have invalid values");
    }
}

std::string Board::to_fen() const {
    std::string result;
    for (int rank = 7; rank >= 0; --rank) {
        int empty_count = 0;
        for (int file = 0; file < 8; ++file) {
            int square = rank * 8 + file;
            auto piece = piece_at(square);
            if (!piece) {
                ++empty_count;
                continue;
            }
            if (empty_count > 0) {
                result += static_cast<char>('0' + empty_count);
                empty_count = 0;
            }
            result += piece_to_char(piece->first, piece->second);
        }
        if (empty_count > 0) {
            result += static_cast<char>('0' + empty_count);
        }
        if (rank > 0) {
            result += '/';
        }
    }

    result += side_to_move_ == Color::White ? " w " : " b ";

    std::string castling;
    if (castling_.white_kingside) castling += 'K';
    if (castling_.white_queenside) castling += 'Q';
    if (castling_.black_kingside) castling += 'k';
    if (castling_.black_queenside) castling += 'q';
    if (castling.empty()) {
        castling = "-";
    }
    result += castling;
    result += ' ';
    result += en_passant_square_ >= 0 ? square_to_string(en_passant_square_) : "-";
    result += ' ' + std::to_string(halfmove_clock_);
    result += ' ' + std::to_string(fullmove_number_);

    return result;
}

bool Board::is_square_attacked(int square, Color by) const {
    if (square < 0 || square >= 64) {
        throw std::out_of_range("Square outside of board");
    }

    const auto &pieces_set = by == Color::White ? white_ : black_;
    Bitboard mask = one_bit(square);

    const Bitboard pawn_attacks = by == Color::White
                                      ? pawn_attacks_white(pieces_set[static_cast<std::size_t>(PieceType::Pawn)])
                                      : pawn_attacks_black(pieces_set[static_cast<std::size_t>(PieceType::Pawn)]);
    if (pawn_attacks & mask) {
        return true;
    }

    if (knight_attacks(square) & pieces_set[static_cast<std::size_t>(PieceType::Knight)]) {
        return true;
    }

    const Bitboard bishops_and_queens =
        pieces_set[static_cast<std::size_t>(PieceType::Bishop)] |
        pieces_set[static_cast<std::size_t>(PieceType::Queen)];
    if (bishop_attacks(square, occupancy_) & bishops_and_queens) {
        return true;
    }

    const Bitboard rooks_and_queens =
        pieces_set[static_cast<std::size_t>(PieceType::Rook)] |
        pieces_set[static_cast<std::size_t>(PieceType::Queen)];
    if (rook_attacks(square, occupancy_) & rooks_and_queens) {
        return true;
    }

    if (king_attacks(square) & pieces_set[static_cast<std::size_t>(PieceType::King)]) {
        return true;
    }

    return false;
}

}  // namespace sirio

