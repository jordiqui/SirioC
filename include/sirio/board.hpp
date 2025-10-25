#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
    using PieceList = std::vector<int>;

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
    [[nodiscard]] const PieceList &piece_list(Color color, PieceType type) const;

    [[nodiscard]] bool is_square_attacked(int square, Color by) const;

private:
    static constexpr std::size_t piece_type_count = static_cast<std::size_t>(PieceType::Count);

    std::array<Bitboard, piece_type_count> white_{};
    std::array<Bitboard, piece_type_count> black_{};
    std::array<std::array<PieceList, piece_type_count>, 2> piece_lists_{};
    Bitboard occupancy_ = 0;
    Color side_to_move_ = Color::White;
    CastlingRights castling_{};
    int halfmove_clock_ = 0;
    int fullmove_number_ = 1;
    int en_passant_square_ = -1;

    [[nodiscard]] Bitboard &pieces_ref(Color color, PieceType type);
    [[nodiscard]] const Bitboard &pieces_ref(Color color, PieceType type) const;
    [[nodiscard]] PieceList &piece_list_ref(Color color, PieceType type);
    [[nodiscard]] const PieceList &piece_list_ref(Color color, PieceType type) const;
    void add_to_piece_list(Color color, PieceType type, int square);
    void remove_from_piece_list(Color color, PieceType type, int square);
    void clear();
    static PieceType piece_type_from_char(char piece);
    static char piece_to_char(Color color, PieceType type);
    static int square_from_string(std::string_view square);
    static std::string square_to_string(int square);
};

}  // namespace sirio

