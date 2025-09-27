#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace engine {

using Move = uint32_t; // TODO: encode from,to,promo,flags
constexpr Move MOVE_NONE = 0;

struct Limits {
    int32_t depth = 64;
    int64_t movetime_ms = -1;
    int64_t wtime_ms = -1, btime_ms = -1, winc_ms = 0, binc_ms = 0;
    int64_t nodes = -1;
};

struct ParsedGo {
    Limits limits;
};

} // namespace engine
