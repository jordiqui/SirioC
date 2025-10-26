#pragma once

#include <optional>

#include "sirio/board.hpp"

namespace sirio {

bool sufficient_material_to_force_checkmate(const Board &board);
std::optional<int> evaluate_specialized_endgame(const Board &board);

}  // namespace sirio

