#pragma once

#include <cstdint>

namespace engine {

class Board;

uint64_t perft(Board& board, int depth);

} // namespace engine

