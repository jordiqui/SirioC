#pragma once

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

struct ProbeResult {
    WdlOutcome wdl = WdlOutcome::Draw;
};

bool init(const std::string& path);
void shutdown();
bool is_available();
std::optional<ProbeResult> probe_wdl(const Board& board);

} // namespace syzygy

} // namespace engine

