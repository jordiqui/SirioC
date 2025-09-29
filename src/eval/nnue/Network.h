#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include "Loader.h"

namespace nnue {

class Network {
public:
    bool load(const std::string &path, std::string &metaInfo);
    bool isLoaded() const { return loaded_.load(); }

    int evaluate(const int16_t *accum, int stm) const;

    int layerSize() const { return static_cast<int>(w_.hdr.layerSize); }

private:
    Weights w_{};
    std::atomic<bool> loaded_{false};
};

extern Network g_network;

} // namespace nnue
