#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "sirio/bitboard.hpp"

namespace sirio {

enum class Color { White, Black };

enum class PieceType { Pawn = 0, Knight, Bishop, Rook, Queen, King, Count };

struct Move;

Color opposite(Color color);

struct CastlingRights {
    bool white_kingside = false;
    bool white_queenside = false;
    bool black_kingside = false;
    bool black_queenside = false;
};

class Board {
public:
    Board();
    explicit Board(std::string_view fen);

    void set_from_fen(std::string_view fen);
    [[nodiscard]] std::string to_fen() const;

    [[nodiscard]] Bitboard pieces(Color color, PieceType type) const;
    [[nodiscard]] Bitboard occupancy(Color color) const;
    [[nodiscard]] Bitboard occupancy() const { return occupancy_; }
    [[nodiscard]] Color side_to_move() const { return side_to_move_; }
    [[nodiscard]] const CastlingRights &castling_rights() const { return castling_; }
    [[nodiscard]] int halfmove_clock() const { return halfmove_clock_; }
    [[nodiscard]] int fullmove_number() const { return fullmove_number_; }
    [[nodiscard]] std::optional<int> en_passant_square() const;
    [[nodiscard]] std::optional<std::pair<Color, PieceType>> piece_at(int square) const;
    [[nodiscard]] int king_square(Color color) const;
    [[nodiscard]] bool in_check(Color color) const;
    [[nodiscard]] Board apply_move(const Move &move) const;

    [[nodiscard]] bool is_square_attacked(int square, Color by) const;

private:
    std::array<Bitboard, static_cast<std::size_t>(PieceType::Count)> white_{};
    std::array<Bitboard, static_cast<std::size_t>(PieceType::Count)> black_{};
    Bitboard occupancy_ = 0;
    Color side_to_move_ = Color::White;
    CastlingRights castling_{};
    int halfmove_clock_ = 0;
    int fullmove_number_ = 1;
    int en_passant_square_ = -1;

    [[nodiscard]] Bitboard &pieces_ref(Color color, PieceType type);
    [[nodiscard]] const Bitboard &pieces_ref(Color color, PieceType type) const;
    void clear();
    static PieceType piece_type_from_char(char piece);
    static char piece_to_char(Color color, PieceType type);
    static int square_from_string(std::string_view square);
    static std::string square_to_string(int square);
};

}  // namespace sirio

