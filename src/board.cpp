#include "sirio/board.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

#include "sirio/move.hpp"
#include "sirio/evaluation.hpp"

namespace sirio {

namespace {
constexpr const char *kStartPositionFEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

constexpr std::size_t piece_count = static_cast<std::size_t>(PieceType::Count);

constexpr std::size_t zobrist_piece_count = piece_count * 2 * 64;

struct ZobristTables {
    std::array<std::uint64_t, zobrist_piece_count> pieces{};
    std::array<std::uint64_t, 4> castling{};
    std::array<std::uint64_t, 8> en_passant{};
    std::uint64_t side_to_move = 0;
};

const ZobristTables &zobrist_tables() {
    static const ZobristTables tables = [] {
        ZobristTables result{};
        std::mt19937_64 rng(0x9E3779B97F4A7C15ULL);
        for (auto &value : result.pieces) {
            value = rng();
        }
        for (auto &value : result.castling) {
            value = rng();
        }
        for (auto &value : result.en_passant) {
            value = rng();
        }
        result.side_to_move = rng();
        return result;
    }();
    return tables;
}

std::uint64_t piece_hash(Color color, PieceType type, int square) {
    const auto &tables = zobrist_tables();
    const std::size_t color_index = color == Color::White ? 0U : 1U;
    const std::size_t type_index = static_cast<std::size_t>(type);
    const std::size_t offset = ((color_index * piece_count) + type_index) * 64 +
                               static_cast<std::size_t>(square);
    return tables.pieces[offset];
}

std::uint64_t castling_hash(Color color, bool kingside) {
    const auto &tables = zobrist_tables();
    std::size_t index = 0;
    if (color == Color::White) {
        index = kingside ? 0 : 1;
    } else {
        index = kingside ? 2 : 3;
    }
    return tables.castling[index];
}

std::uint64_t en_passant_hash(int file) {
    const auto &tables = zobrist_tables();
    return tables.en_passant[static_cast<std::size_t>(file)];
}

std::uint64_t side_to_move_hash() { return zobrist_tables().side_to_move; }

bool en_passant_capture_possible(const Board &board, int ep_square, Color capturer) {
    if (ep_square < 0) {
        return false;
    }

    const Bitboard pawns = board.pieces(capturer, PieceType::Pawn);
    const int file = file_of(ep_square);

    auto has_pawn_on = [&](int square, int expected_file) {
        if (square < 0 || square >= 64) {
            return false;
        }
        if (file_of(square) != expected_file) {
            return false;
        }
        return (pawns & one_bit(square)) != 0;
    };

    if (capturer == Color::White) {
        if (file > 0 && has_pawn_on(ep_square - 9, file - 1)) {
            return true;
        }
        if (file < 7 && has_pawn_on(ep_square - 7, file + 1)) {
            return true;
        }
    } else {
        if (file > 0 && has_pawn_on(ep_square + 7, file - 1)) {
            return true;
        }
        if (file < 7 && has_pawn_on(ep_square + 9, file + 1)) {
            return true;
        }
    }

    return false;
}
}  // namespace

Color opposite(Color color) {
    return color == Color::White ? Color::Black : Color::White;
}

Board::Board() {
    set_from_fen(kStartPositionFEN);
}

Board::Board(std::string_view fen) {
    set_from_fen(fen);
}

void Board::clear() {
    white_.fill(0);
    black_.fill(0);
    for (auto &color_lists : piece_lists_) {
        for (auto &list : color_lists) {
            list.clear();
        }
    }
    occupancy_ = 0;
    state_ = {};
    history_.clear();
}

Bitboard &Board::pieces_ref(Color color, PieceType type) {
    auto index = static_cast<std::size_t>(type);
    return color == Color::White ? white_[index] : black_[index];
}

const Bitboard &Board::pieces_ref(Color color, PieceType type) const {
    auto index = static_cast<std::size_t>(type);
    return color == Color::White ? white_[index] : black_[index];
}

Board::PieceList &Board::piece_list_ref(Color color, PieceType type) {
    auto color_index = color == Color::White ? 0 : 1;
    auto type_index = static_cast<std::size_t>(type);
    return piece_lists_[color_index][type_index];
}

const Board::PieceList &Board::piece_list_ref(Color color, PieceType type) const {
    auto color_index = color == Color::White ? 0 : 1;
    auto type_index = static_cast<std::size_t>(type);
    return piece_lists_[color_index][type_index];
}

void Board::add_to_piece_list(Color color, PieceType type, int square) {
    piece_list_ref(color, type).push_back(square);
}

void Board::remove_from_piece_list(Color color, PieceType type, int square) {
    auto &list = piece_list_ref(color, type);
    auto it = std::find(list.begin(), list.end(), square);
    if (it == list.end()) {
        throw std::invalid_argument("Piece list missing square");
    }
    list.erase(it);
}

Bitboard Board::pieces(Color color, PieceType type) const { return pieces_ref(color, type); }

Bitboard Board::occupancy(Color color) const {
    const auto &source = color == Color::White ? white_ : black_;
    Bitboard occ = 0;
    for (Bitboard bb : source) {
        occ |= bb;
    }
    return occ;
}

bool Board::has_bishop_pair(Color color) const {
    const auto &bishops = piece_list(color, PieceType::Bishop);
    if (bishops.size() < 2) {
        return false;
    }

    bool has_light_square = false;
    bool has_dark_square = false;
    for (int square : bishops) {
        const bool is_light_square = ((file_of(square) + rank_of(square)) & 1) != 0;
        if (is_light_square) {
            has_light_square = true;
        } else {
            has_dark_square = true;
        }

        if (has_light_square && has_dark_square) {
            return true;
        }
    }

    return false;
}

std::optional<int> Board::en_passant_square() const {
    if (state_.en_passant_square < 0) {
        return std::nullopt;
    }
    return state_.en_passant_square;
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

int Board::king_square(Color color) const {
    Bitboard kings = pieces(color, PieceType::King);
    if (kings == 0) {
        return -1;
    }
    return bit_scan_forward(kings);
}

bool Board::in_check(Color color) const {
    const int king_sq = king_square(color);
    if (king_sq < 0) {
        return false;
    }
    return is_square_attacked(king_sq, opposite(color));
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

const Board::PieceList &Board::piece_list(Color color, PieceType type) const {
    return piece_list_ref(color, type);
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
        add_to_piece_list(color, type, square);
        occupancy_ |= mask;
        state_.zobrist_hash ^= piece_hash(color, type, square);
        ++file;
    }

    if (rank != 0 || file != 8) {
        throw std::invalid_argument("FEN placement does not describe all squares");
    }

    if (active_color == "w") {
        state_.side_to_move = Color::White;
    } else if (active_color == "b") {
        state_.side_to_move = Color::Black;
        state_.zobrist_hash ^= side_to_move_hash();
    } else {
        throw std::invalid_argument("Invalid active color in FEN");
    }

    if (castling_rights_text == "-") {
        state_.castling = {};
    } else {
        for (char c : castling_rights_text) {
            switch (c) {
                case 'K':
                    if (!state_.castling.white_kingside) {
                        state_.castling.white_kingside = true;
                        state_.zobrist_hash ^= castling_hash(Color::White, true);
                    }
                    break;
                case 'Q':
                    if (!state_.castling.white_queenside) {
                        state_.castling.white_queenside = true;
                        state_.zobrist_hash ^= castling_hash(Color::White, false);
                    }
                    break;
                case 'k':
                    if (!state_.castling.black_kingside) {
                        state_.castling.black_kingside = true;
                        state_.zobrist_hash ^= castling_hash(Color::Black, true);
                    }
                    break;
                case 'q':
                    if (!state_.castling.black_queenside) {
                        state_.castling.black_queenside = true;
                        state_.zobrist_hash ^= castling_hash(Color::Black, false);
                    }
                    break;
                default:
                    throw std::invalid_argument("Invalid castling rights in FEN");
            }
        }
    }

    if (en_passant_text == "-") {
        state_.en_passant_square = -1;
    } else {
        state_.en_passant_square = square_from_string(en_passant_text);
        if (state_.en_passant_square < 0) {
            throw std::invalid_argument("Invalid en passant square in FEN");
        }
        if (en_passant_capture_possible(*this, state_.en_passant_square, state_.side_to_move)) {
            state_.zobrist_hash ^= en_passant_hash(file_of(state_.en_passant_square));
        }
    }

    try {
        state_.halfmove_clock = std::stoi(halfmove_text);
        state_.fullmove_number = std::stoi(fullmove_text);
    } catch (const std::exception &) {
        throw std::invalid_argument("Invalid move counters in FEN");
    }

    if (state_.halfmove_clock < 0 || state_.fullmove_number <= 0) {
        throw std::invalid_argument("FEN counters have invalid values");
    }

    history_.push(state_);
    notify_position_initialization(*this);
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

    result += state_.side_to_move == Color::White ? " w " : " b ";

    std::string castling;
    if (state_.castling.white_kingside) castling += 'K';
    if (state_.castling.white_queenside) castling += 'Q';
    if (state_.castling.black_kingside) castling += 'k';
    if (state_.castling.black_queenside) castling += 'q';
    if (castling.empty()) {
        castling = "-";
    }
    result += castling;
    result += ' ';
    result += state_.en_passant_square >= 0 ? square_to_string(state_.en_passant_square) : "-";
    result += ' ' + std::to_string(state_.halfmove_clock);
    result += ' ' + std::to_string(state_.fullmove_number);

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

Board Board::apply_null_move() const {
    Board result = *this;
    const Color us = state_.side_to_move;
    const Color them = opposite(us);

    if (result.state_.en_passant_square >= 0) {
        if (en_passant_capture_possible(result, result.state_.en_passant_square, us)) {
            result.state_.zobrist_hash ^=
                en_passant_hash(file_of(result.state_.en_passant_square));
        }
        result.state_.en_passant_square = -1;
    }

    result.state_.side_to_move = them;
    result.state_.zobrist_hash ^= side_to_move_hash();
    ++result.state_.halfmove_clock;
    if (us == Color::Black) {
        ++result.state_.fullmove_number;
    }

    result.history_.push(result.state_);
    notify_move_applied(*this, std::nullopt, result);
    return result;
}

Board Board::apply_move(const Move &move) const {
    Board result = *this;
    const Color us = state_.side_to_move;
    const Color them = opposite(us);
    const Bitboard from_mask = one_bit(move.from);
    const Bitboard to_mask = one_bit(move.to);

    auto remove_piece_hash = [&](Color color, PieceType type, int square) {
        result.state_.zobrist_hash ^= piece_hash(color, type, square);
    };

    auto add_piece_hash = [&](Color color, PieceType type, int square) {
        result.state_.zobrist_hash ^= piece_hash(color, type, square);
    };

    auto clear_en_passant_hash = [&]() {
        if (result.state_.en_passant_square >= 0) {
            if (en_passant_capture_possible(result, result.state_.en_passant_square, us)) {
                result.state_.zobrist_hash ^=
                    en_passant_hash(file_of(result.state_.en_passant_square));
            }
            result.state_.en_passant_square = -1;
        }
    };

    Bitboard &moving_bb = result.pieces_ref(us, move.piece);
    if ((moving_bb & from_mask) == 0) {
        throw std::invalid_argument("Move does not match board state");
    }
    moving_bb &= ~from_mask;
    result.remove_from_piece_list(us, move.piece, move.from);
    remove_piece_hash(us, move.piece, move.from);

    clear_en_passant_hash();

    auto reset_castling_rights = [&](Color color) {
        auto disable_castling = [&](Color target, bool kingside) {
            bool &right = [&]() -> bool & {
                if (target == Color::White) {
                    return kingside ? result.state_.castling.white_kingside
                                    : result.state_.castling.white_queenside;
                }
                return kingside ? result.state_.castling.black_kingside
                                : result.state_.castling.black_queenside;
            }();
            if (right) {
                result.state_.zobrist_hash ^= castling_hash(target, kingside);
                right = false;
            }
        };
        if (color == Color::White) {
            disable_castling(Color::White, true);
            disable_castling(Color::White, false);
        } else {
            disable_castling(Color::Black, true);
            disable_castling(Color::Black, false);
        }
    };

    if (move.piece == PieceType::King) {
        reset_castling_rights(us);
    }

    auto update_rook_rights_on_move = [&](Color color, int square) {
        auto disable_castling = [&](Color target, bool kingside) {
            bool &right = [&]() -> bool & {
                if (target == Color::White) {
                    return kingside ? result.state_.castling.white_kingside
                                    : result.state_.castling.white_queenside;
                }
                return kingside ? result.state_.castling.black_kingside
                                : result.state_.castling.black_queenside;
            }();
            if (right) {
                result.state_.zobrist_hash ^= castling_hash(target, kingside);
                right = false;
            }
        };
        if (color == Color::White) {
            if (square == 0) {
                disable_castling(Color::White, false);
            } else if (square == 7) {
                disable_castling(Color::White, true);
            }
        } else {
            if (square == 56) {
                disable_castling(Color::Black, false);
            } else if (square == 63) {
                disable_castling(Color::Black, true);
            }
        }
    };

    if (move.piece == PieceType::Rook) {
        update_rook_rights_on_move(us, move.from);
    }

    bool is_capture = false;

    if (move.is_en_passant) {
        const int capture_square = us == Color::White ? move.to - 8 : move.to + 8;
        Bitboard &capture_bb = result.pieces_ref(them, PieceType::Pawn);
        Bitboard capture_mask = one_bit(capture_square);
        if ((capture_bb & capture_mask) == 0) {
            throw std::invalid_argument("En passant capture missing pawn");
        }
        capture_bb &= ~capture_mask;
        result.remove_from_piece_list(them, PieceType::Pawn, capture_square);
        remove_piece_hash(them, PieceType::Pawn, capture_square);
        is_capture = true;
    } else if (move.captured.has_value()) {
        Bitboard &capture_bb = result.pieces_ref(them, *move.captured);
        if ((capture_bb & to_mask) == 0) {
            throw std::invalid_argument("Capture square empty");
        }
        capture_bb &= ~to_mask;
        update_rook_rights_on_move(them, move.to);
        result.remove_from_piece_list(them, *move.captured, move.to);
        remove_piece_hash(them, *move.captured, move.to);
        is_capture = true;
    }

    PieceType placed_piece = move.promotion.has_value() ? *move.promotion : move.piece;
    result.pieces_ref(us, placed_piece) |= to_mask;
    result.add_to_piece_list(us, placed_piece, move.to);
    add_piece_hash(us, placed_piece, move.to);

    if (move.is_castling) {
        int rook_from;
        int rook_to;
        if (move.to > move.from) {  // Kingside
            rook_from = us == Color::White ? 7 : 63;
            rook_to = us == Color::White ? 5 : 61;
        } else {  // Queenside
            rook_from = us == Color::White ? 0 : 56;
            rook_to = us == Color::White ? 3 : 59;
        }
        Bitboard rook_from_mask = one_bit(rook_from);
        Bitboard rook_to_mask = one_bit(rook_to);
        Bitboard &rook_bb = result.pieces_ref(us, PieceType::Rook);
        if ((rook_bb & rook_from_mask) == 0) {
            throw std::invalid_argument("Castling rook missing");
        }
        rook_bb &= ~rook_from_mask;
        rook_bb |= rook_to_mask;
        result.remove_from_piece_list(us, PieceType::Rook, rook_from);
        result.add_to_piece_list(us, PieceType::Rook, rook_to);
        remove_piece_hash(us, PieceType::Rook, rook_from);
        add_piece_hash(us, PieceType::Rook, rook_to);
    }

    if (move.piece == PieceType::Pawn) {
        result.state_.halfmove_clock = 0;
        if (std::abs(move.to - move.from) == 16) {
            result.state_.en_passant_square = us == Color::White ? move.from + 8 : move.from - 8;
            if (en_passant_capture_possible(result, result.state_.en_passant_square, them)) {
                result.state_.zobrist_hash ^=
                    en_passant_hash(file_of(result.state_.en_passant_square));
            }
        }
    } else if (is_capture) {
        result.state_.halfmove_clock = 0;
    } else {
        ++result.state_.halfmove_clock;
    }

    result.state_.side_to_move = them;
    result.state_.zobrist_hash ^= side_to_move_hash();
    if (us == Color::Black) {
        ++result.state_.fullmove_number;
    }

    result.occupancy_ = 0;
    for (std::size_t index = 0; index < piece_count; ++index) {
        result.occupancy_ |= result.white_[index];
        result.occupancy_ |= result.black_[index];
    }

    result.history_.push(result.state_);

    notify_move_applied(*this, std::optional<Move>{move}, result);
    return result;
}

}  // namespace sirio

