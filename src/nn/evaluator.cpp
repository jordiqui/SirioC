#include "nn/evaluator.h"

extern "C" {
#include "nn/accumulator.h"
}

namespace sirio::nn {

namespace {

std::size_t index(pyrrhic::PieceType type) {
    return static_cast<std::size_t>(type);
}

}  // namespace

Evaluator::Evaluator() {
    sirio_nn_model_init(&model_);
}

int Evaluator::evaluate(const pyrrhic::Board& board) const {
    sirio_accumulator accumulator;
    sirio_accumulator_reset(&accumulator);

    for (const auto& square : board.squares()) {
        if (!square.has_value()) {
            continue;
        }

        const auto color = square->color == pyrrhic::Color::White ? SIRIO_NN_COLOR_WHITE : SIRIO_NN_COLOR_BLACK;
        sirio_accumulator_add(&accumulator, color, static_cast<int>(index(square->type)), 1);
    }

    return sirio_nn_evaluate(&model_, &accumulator);
}

bool Evaluator::load_network(const std::string& path) {
    return sirio_nn_model_load(&model_, path.c_str()) != 0;
}

void Evaluator::set_piece_value(pyrrhic::PieceType type, int value) {
    sirio_nn_model_set_weight(&model_, static_cast<int>(index(type)), value);
}

int Evaluator::piece_value(pyrrhic::PieceType type) const {
    return sirio_nn_model_weight(&model_, static_cast<int>(index(type)));
}

}  // namespace sirio::nn
