#pragma once

#include <optional>
#include <string>

#include "sirio/board.hpp"
#include "sirio/move.hpp"

namespace sirio::syzygy {

struct ProbeResult {
    int wdl = 0;
    int dtz = 0;
    std::optional<Move> best_move;
};

void set_tablebase_path(const std::string &path);
[[nodiscard]] const std::string &tablebase_path();
[[nodiscard]] bool available();
[[nodiscard]] int max_pieces();
[[nodiscard]] std::optional<ProbeResult> probe_wdl(const Board &board);
[[nodiscard]] std::optional<ProbeResult> probe_root(const Board &board);

}  // namespace sirio::syzygy

