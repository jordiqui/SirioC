#pragma once

#include "engine/types.hpp"

#include <optional>
#include <string>

namespace engine {

class Board;

namespace syzygy {

enum class WdlOutcome : int {
    Loss = 0,
    BlessedLoss = 1,
    Draw = 2,
    CursedWin = 3,
    Win = 4
};

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

} // namespace TB

} // namespace syzygy

} // namespace engine

