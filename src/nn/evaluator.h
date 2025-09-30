#pragma once

#include "pyrrhic/board.h"

#include <array>

namespace sirio::nn {

class Evaluator {
public:
    Evaluator();

    int evaluate(const pyrrhic::Board& board) const;
    void set_piece_value(pyrrhic::PieceType type, int value);
    int piece_value(pyrrhic::PieceType type) const;

private:
    std::array<int, 6> piece_values_;
};

}  // namespace sirio::nn
