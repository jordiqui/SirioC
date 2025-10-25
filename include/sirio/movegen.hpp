#pragma once

#include <vector>

#include "sirio/board.hpp"
#include "sirio/move.hpp"

namespace sirio {

std::vector<Move> generate_pseudo_legal_moves(const Board &board);
std::vector<Move> generate_legal_moves(const Board &board);

}  // namespace sirio

