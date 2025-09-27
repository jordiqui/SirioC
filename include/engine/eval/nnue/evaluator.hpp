#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "engine/eval/nnue/accumulator.hpp"

namespace engine { class Board; }

namespace engine::nnue {

struct FeatureTransformer {
    uint32_t input_dim = 0;
    uint32_t output_dim = 0;
    std::vector<int16_t> weights;  // layout: output-major
    std::vector<int32_t> bias;

    [[nodiscard]] bool valid() const noexcept {
        return output_dim == bias.size() &&
               weights.size() == static_cast<std::size_t>(input_dim) * static_cast<std::size_t>(output_dim);
    }
};

struct OutputLayer {
    std::vector<int16_t> weights;
    int32_t bias = 0;
    int32_t scale = 1;

    [[nodiscard]] bool valid(std::size_t expected_size) const noexcept {
        return scale != 0 && weights.size() == expected_size;
    }
};

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
        FeatureTransformer transformer;
        OutputLayer output;
    } network_;

    mutable Accumulator accumulator_;
    bool loaded_ = false;
    std::string last_error_;
};

} // namespace engine::nnue
