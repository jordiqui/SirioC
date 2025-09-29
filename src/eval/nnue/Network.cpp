#include "Network.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace nnue {

namespace {
constexpr int kClipLo = 0;
constexpr int kClipHi = 255;

inline int clipped_relu(int value) {
    return std::clamp(value, kClipLo, kClipHi);
}

} // namespace

Network g_network;

bool Network::load(const std::string &path, std::string &metaInfo) {
    Weights weights;
    std::string info;
    if (!loadFile(path, weights, info)) {
        loaded_.store(false);
        return false;
    }

    w_ = std::move(weights);
    metaInfo = std::move(info);
    loaded_.store(true);
    return true;
}

int Network::evaluate(const int16_t *accum, int /*stm*/) const {
    if (!isLoaded() || accum == nullptr) {
        return 0;
    }

    const int size = layerSize();
    if (size <= 0) {
        return 0;
    }

    int32_t total = 0;
    for (int i = 0; i < size; ++i) {
        int val = accum[i];
        if (i < static_cast<int>(w_.ftBias.size())) {
            val += w_.ftBias[static_cast<std::size_t>(i)];
        }
        val = clipped_relu(val);
        if (i < static_cast<int>(w_.outWeights.size())) {
            total += val * w_.outWeights[static_cast<std::size_t>(i)];
        }
    }

    if (!w_.outBias.empty()) {
        total += w_.outBias.front();
    }

    // Scale back to centipawns with a conservative factor.
    return static_cast<int>(total / 16);
}

} // namespace nnue
