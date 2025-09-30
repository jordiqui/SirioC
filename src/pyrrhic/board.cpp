#include "pyrrhic/board.h"

#include <array>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace sirio::pyrrhic {

namespace {

constexpr std::array<int, 8> KNIGHT_OFFSETS = {17, 15, 10, 6, -17, -15, -10, -6};
constexpr std::array<int, 8> KING_OFFSETS = {8, 7, 9, 1, -8, -7, -9, -1};
constexpr std::array<int, 4> BISHOP_OFFSETS = {9, 7, -9, -7};
constexpr std::array<int, 4> ROOK_OFFSETS = {8, -8, 1, -1};

bool is_knight_move(int from, int to) {
    const int file_delta = std::abs(file_of(from) - file_of(to));
    const int rank_delta = std::abs(rank_of(from) - rank_of(to));
    return (file_delta == 1 && rank_delta == 2) || (file_delta == 2 && rank_delta == 1);
}

bool is_valid_destination(int from, int to, int offset) {
    if (!is_valid_square(to)) {
        return false;
    }

    if (std::abs(offset) == 1) {
        return rank_of(from) == rank_of(to);
    }

    if (std::abs(offset) == 8) {
        return file_of(from) == file_of(to);
    }

    if (std::abs(offset) == 7 || std::abs(offset) == 9) {
        return std::abs(file_of(from) - file_of(to)) == 1;
    }

    return true;
}

}  // namespace

std::string Move::to_string() const {
    const auto from_file = static_cast<char>('a' + file_of(from));
    const auto from_rank = static_cast<char>('1' + rank_of(from));
    const auto to_file = static_cast<char>('a' + file_of(to));
    const auto to_rank = static_cast<char>('1' + rank_of(to));

    std::string algebraic;
    algebraic.push_back(from_file);
    algebraic.push_back(from_rank);
    algebraic.push_back(to_file);
    algebraic.push_back(to_rank);

    if (capture.has_value()) {
        algebraic.push_back('x');
        algebraic.push_back(piece_to_char(*capture));
    }

    return algebraic;
}

Board::Board() {
    clear();
}

void Board::clear() {
    squares_.fill(std::nullopt);
    side_to_move_ = Color::White;
    castling_rights_ = "-";
    en_passant_.reset();
    halfmove_clock_ = 0;
    fullmove_number_ = 1;
}

void Board::set_piece(int square, std::optional<Piece> piece) {
    if (!is_valid_square(square)) {
        throw std::out_of_range("Square index out of range");
    }
    squares_.at(static_cast<std::size_t>(square)) = piece;
}

std::optional<Piece> Board::piece_at(int square) const {
    if (!is_valid_square(square)) {
        throw std::out_of_range("Square index out of range");
    }
    return squares_.at(static_cast<std::size_t>(square));
}

Color Board::side_to_move() const {
    return side_to_move_;
}

void Board::set_side_to_move(Color color) {
    side_to_move_ = color;
}

std::string Board::castling_rights() const {
    return castling_rights_;
}

void Board::set_castling_rights(std::string rights) {
    castling_rights_ = std::move(rights);
    if (castling_rights_.empty()) {
        castling_rights_ = "-";
    }
}

std::optional<int> Board::en_passant_square() const {
    return en_passant_;
}

void Board::set_en_passant_square(std::optional<int> square) {
    en_passant_ = square;
}

int Board::halfmove_clock() const {
    return halfmove_clock_;
}

void Board::set_halfmove_clock(int value) {
    halfmove_clock_ = value;
}

int Board::fullmove_number() const {
    return fullmove_number_;
}

void Board::set_fullmove_number(int value) {
    fullmove_number_ = value;
}

std::vector<Move> Board::generate_basic_moves() const {
    std::vector<Move> moves;
    const auto active_color = side_to_move_;

    const auto add_move = [&](int from, int to) {
        if (!is_valid_square(to)) {
            return;
        }
        const auto& target = squares_.at(static_cast<std::size_t>(to));
        if (target.has_value() && target->color == active_color) {
            return;
        }
        moves.push_back(Move{from, to, target});
    };

    for (int square = 0; square < 64; ++square) {
        const auto& piece = squares_.at(static_cast<std::size_t>(square));
        if (!piece.has_value() || piece->color != active_color) {
            continue;
        }

        switch (piece->type) {
            case PieceType::Pawn: {
                const int direction = active_color == Color::White ? 8 : -8;
                const int start_rank = active_color == Color::White ? 1 : 6;
                const int square_rank = rank_of(square);

                const int forward = square + direction;
                if (is_valid_square(forward) && !squares_.at(static_cast<std::size_t>(forward)).has_value()) {
                    add_move(square, forward);
                    const int double_forward = forward + direction;
                    if (square_rank == start_rank &&
                        is_valid_square(double_forward) &&
                        !squares_.at(static_cast<std::size_t>(double_forward)).has_value()) {
                        add_move(square, double_forward);
                    }
                }

                const int capture_left = square + direction - 1;
                if (is_valid_destination(square, capture_left, direction - 1)) {
                    const auto& target = is_valid_square(capture_left)
                                             ? squares_.at(static_cast<std::size_t>(capture_left))
                                             : std::nullopt;
                    if (target.has_value() && target->color != active_color) {
                        add_move(square, capture_left);
                    }
                }

                const int capture_right = square + direction + 1;
                if (is_valid_destination(square, capture_right, direction + 1)) {
                    const auto& target = is_valid_square(capture_right)
                                             ? squares_.at(static_cast<std::size_t>(capture_right))
                                             : std::nullopt;
                    if (target.has_value() && target->color != active_color) {
                        add_move(square, capture_right);
                    }
                }
                break;
            }
            case PieceType::Knight: {
                for (int offset : KNIGHT_OFFSETS) {
                    const int target_square = square + offset;
                    if (!is_valid_square(target_square)) {
                        continue;
                    }
                    if (!is_knight_move(square, target_square)) {
                        continue;
                    }
                    add_move(square, target_square);
                }
                break;
            }
            case PieceType::Bishop:
            case PieceType::Rook:
            case PieceType::Queen: {
                const auto select_offsets = [&]() {
                    if (piece->type == PieceType::Bishop) {
                        return std::vector<int>(BISHOP_OFFSETS.begin(), BISHOP_OFFSETS.end());
                    }
                    if (piece->type == PieceType::Rook) {
                        return std::vector<int>(ROOK_OFFSETS.begin(), ROOK_OFFSETS.end());
                    }
                    return std::vector<int>{8, -8, 1, -1, 9, 7, -9, -7};
                }();
                for (int offset : select_offsets) {
                    int previous_square = square;
                    int target_square = square + offset;
                    while (is_valid_square(target_square) &&
                           is_valid_destination(previous_square, target_square, offset)) {
                        const auto& target = squares_.at(static_cast<std::size_t>(target_square));
                        add_move(square, target_square);
                        if (target.has_value()) {
                            break;
                        }
                        previous_square = target_square;
                        target_square += offset;
                    }
                }
                break;
            }
            case PieceType::King: {
                for (int offset : KING_OFFSETS) {
                    const int target_square = square + offset;
                    if (!is_valid_destination(square, target_square, offset)) {
                        continue;
                    }
                    add_move(square, target_square);
                }
                break;
            }
        }
    }

    return moves;
}

std::string Board::pretty() const {
    std::ostringstream output;
    for (int rank = 7; rank >= 0; --rank) {
        output << (rank + 1) << " ";
        for (int file = 0; file < 8; ++file) {
            const int square = make_square(file, rank);
            const auto& piece = squares_.at(static_cast<std::size_t>(square));
            if (piece.has_value()) {
                output << piece_to_char(*piece);
            } else {
                output << '.';
            }
            output << ' ';
        }
        output << '\n';
    }
    output << "  a b c d e f g h\n";
    output << "Side to move: " << color_to_string(side_to_move_) << '\n';
    output << "Castling: " << castling_rights_ << '\n';
    if (en_passant_.has_value()) {
        const auto file = static_cast<char>('a' + file_of(*en_passant_));
        const auto rank = static_cast<char>('1' + rank_of(*en_passant_));
        output << "En passant: " << file << rank << '\n';
    } else {
        output << "En passant: -\n";
    }
    output << "Halfmove clock: " << halfmove_clock_ << '\n';
    output << "Fullmove number: " << fullmove_number_ << '\n';
    return output.str();
}

int file_of(int square) {
    return square % 8;
}

int rank_of(int square) {
    return square / 8;
}

bool is_valid_square(int square) {
    return square >= 0 && square < 64;
}

int make_square(int file, int rank) {
    if (file < 0 || file >= 8 || rank < 0 || rank >= 8) {
        throw std::out_of_range("Square coordinates out of range");
    }
    return rank * 8 + file;
}

}  // namespace sirio::pyrrhic
