#pragma once

#include <array>

#include "engine/core/board.hpp"
#include "engine/types.hpp"

namespace engine::eval {

inline constexpr std::array<int, 6> kSeePieceValues = {100, 320, 330, 500, 900, 0};

int see(const Board& board, Move move);

} // namespace engine::eval

