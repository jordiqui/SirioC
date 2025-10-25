#pragma once

#include "sirio/board.hpp"

namespace sirio {

constexpr int fifty_move_rule_limit = 100;

[[nodiscard]] bool draw_by_fifty_move_rule(const Board &board);

[[nodiscard]] int draw_by_repetition_rule(const Board &board);

[[nodiscard]] bool draw_by_threefold_repetition(const Board &board);

[[nodiscard]] bool draw_by_insufficient_material_rule(const Board &board);

}  // namespace sirio

