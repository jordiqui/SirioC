#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "engine/eval/nnue/accumulator.hpp"

namespace engine { class Board; }

namespace engine::nnue {

class Evaluator {
public:
    bool load_network(const std::string& path);
    int eval_cp(const engine::Board& board) const;

    bool is_loaded() const noexcept { return loaded_; }
    const std::string& last_error() const noexcept { return last_error_; }

private:
    struct Network {
        uint32_t version = 0;
        std::string architecture;
        uint32_t input_dim = 0;
        uint32_t hidden_dim = 0;
        int32_t output_scale = 1;
        std::vector<int16_t> feature_weights;
        std::vector<int32_t> feature_bias;
        std::vector<int16_t> output_weights;
        int32_t output_bias = 0;
    } network_;

    mutable Accumulator accumulator_;
    bool loaded_ = false;
    std::string last_error_;
};

} // namespace engine::nnue
