#include "engine/eval/nnue/evaluator.hpp"

#include <fstream>

#include "engine/core/board.hpp"
#include "engine/eval/nnue/accumulator.hpp"

namespace engine::nnue {

bool Evaluator::load_network(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        loaded_ = false;
        raw_network_.clear();
        return false;
    }
    raw_network_.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    loaded_ = !raw_network_.empty();
    return loaded_;
}

int Evaluator::eval_cp(const engine::Board& board) const {
    return board.nnue_accumulator().evaluate(board.white_to_move());
}

} // namespace engine::nnue

