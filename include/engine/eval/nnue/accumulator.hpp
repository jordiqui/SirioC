#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace engine { class Board; }

namespace engine::nnue {

struct FeatureTransformer;

class Accumulator {
public:
    void reset() noexcept;

    void update(const engine::Board& board, const FeatureTransformer& transformer);

    const std::vector<int32_t>& hidden() const noexcept { return hidden_; }

private:
    std::vector<int32_t> hidden_;
    std::vector<int32_t> features_;
    std::string cached_fen_;
    uint32_t cached_input_dim_ = 0;
    uint32_t cached_hidden_dim_ = 0;
    bool initialized_ = false;
};

} // namespace engine::nnue
