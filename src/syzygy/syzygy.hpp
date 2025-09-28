#pragma once

#include "engine/types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace engine {

class Board;

namespace syzygy {

enum class TBProbe : uint8_t {
    None = 0,
    Wdl = 1 << 0,
    Root = 1 << 1
};

codex/replace-engine-syzygy-with-tbconfig-functions
inline TBProbe operator|(TBProbe lhs, TBProbe rhs) {
    return static_cast<TBProbe>(static_cast<uint8_t>(lhs) |
                                static_cast<uint8_t>(rhs));
}

inline TBProbe operator&(TBProbe lhs, TBProbe rhs) {
    return static_cast<TBProbe>(static_cast<uint8_t>(lhs) &
                                static_cast<uint8_t>(rhs));
}

inline TBProbe& operator|=(TBProbe& lhs, TBProbe rhs) {
    lhs = lhs | rhs;
    return lhs;
}

inline bool any(TBProbe value, TBProbe mask) {
    return static_cast<uint8_t>(value & mask) != 0;
}

struct TBConfig {
    bool enabled = false;
    std::string path;
    int probeDepth = 0;
    int probeLimit = 0;
    bool useRule50 = true;
};

struct TBMoveInfo {
    Move move = MOVE_NONE;
    int score = 0;
    int rank = 0;
    int wdl = 0;
    int dtz = 0;
};

struct TBResult {
    TBProbe probe = TBProbe::None;
    int wdl = 0;
    int dtz = 0;
    Move bestMove = MOVE_NONE;
    int tbScore = 0;
    std::vector<TBMoveInfo> moves;
};

namespace TB {

bool init(const TBConfig& config);
void release();
int pieceCount();
std::optional<TBResult> probePosition(const Board& board, TBProbe probeType,
                                      int searchDepth);
=======
struct TBConfig {
    bool enabled = false;
    std::string path;
    int probe_depth = 1;
    int probe_limit = 7;
    bool use_rule50 = true;
};

bool configure(const TBConfig& config);
void shutdown();
bool is_available();

namespace TB {

struct ProbeResult {
    WdlOutcome wdl = WdlOutcome::Draw;
    std::optional<int> dtz;
    std::optional<Move> best_move;
};

int pieceCount(const Board& board);
std::optional<ProbeResult> probePosition(const Board& board, const TBConfig& config,
                                         bool root);
 main

} // namespace TB

} // namespace syzygy

} // namespace engine
