#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace engine { class Board; }

namespace engine::nnue {

class Accumulator {
public:
    void reset() noexcept;

    void update(const engine::Board& board,
                uint32_t input_dim,
                uint32_t hidden_dim,
                const std::vector<int16_t>& feature_weights,
                const std::vector<int32_t>& feature_bias);

    const std::vector<int32_t>& hidden() const noexcept { return hidden_; }

private:
    std::vector<int32_t> hidden_;
    std::string cached_fen_;
    uint32_t cached_input_dim_ = 0;
    uint32_t cached_hidden_dim_ = 0;
};

} // namespace engine::nnue
