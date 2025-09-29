#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nnue {

struct Header {
    uint32_t magic{};
    uint32_t version{};
    uint32_t hash{};
    uint32_t inputSize{};
    uint32_t layerSize{};
    uint32_t output{};
};

struct Weights {
    Header hdr{};
    std::vector<int16_t> ftBias;
    std::vector<int8_t> ftWeights;
    std::vector<int16_t> l1Bias;
    std::vector<int8_t> l1Weights;
    std::vector<int16_t> outBias;
    std::vector<int8_t> outWeights;
};

bool loadFile(const std::string &path, Weights &out, std::string &metaInfo);

} // namespace nnue
