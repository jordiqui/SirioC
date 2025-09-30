#pragma once

#include "pyrrhic/types.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace sirio::pyrrhic {

struct Move {
    int from;
    int to;
    std::optional<Piece> capture;

    std::string to_string() const;
};

class Board {
public:
    Board();

    void clear();
    void set_piece(int square, std::optional<Piece> piece);
    std::optional<Piece> piece_at(int square) const;

    Color side_to_move() const;
    void set_side_to_move(Color color);

    std::string castling_rights() const;
    void set_castling_rights(std::string rights);

    std::optional<int> en_passant_square() const;
    void set_en_passant_square(std::optional<int> square);

    int halfmove_clock() const;
    void set_halfmove_clock(int value);

    int fullmove_number() const;
    void set_fullmove_number(int value);

    std::vector<Move> generate_basic_moves() const;
    std::string pretty() const;

    const std::array<std::optional<Piece>, 64>& squares() const { return squares_; }

private:
    std::array<std::optional<Piece>, 64> squares_{};
    Color side_to_move_{Color::White};
    std::string castling_rights_ = "KQkq";
    std::optional<int> en_passant_;
    int halfmove_clock_ = 0;
    int fullmove_number_ = 1;
};

int file_of(int square);
int rank_of(int square);
bool is_valid_square(int square);
int make_square(int file, int rank);

}  // namespace sirio::pyrrhic
