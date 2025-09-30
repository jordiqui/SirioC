#include "nn/evaluator.h"

#include <numeric>

namespace sirio::nn {

namespace {

std::size_t index(pyrrhic::PieceType type) {
    return static_cast<std::size_t>(type);
}

}  // namespace

Evaluator::Evaluator()
    : piece_values_{100, 320, 330, 500, 900, 20000} {}

int Evaluator::evaluate(const pyrrhic::Board& board) const {
    int score = 0;
    for (const auto& square : board.squares()) {
        if (!square.has_value()) {
            continue;
        }
        const int value = piece_values_.at(index(square->type));
        score += square->color == pyrrhic::Color::White ? value : -value;
    }
    return score;
}

void Evaluator::set_piece_value(pyrrhic::PieceType type, int value) {
    piece_values_.at(index(type)) = value;
}

int Evaluator::piece_value(pyrrhic::PieceType type) const {
    return piece_values_.at(index(type));
}

}  // namespace sirio::nn
