#pragma once

#include <cstdint>
#include <vector>

namespace nnue {

struct Accumulator {
    std::vector<int16_t> buf;
    void clear(int layerSize);
};

} // namespace nnue
