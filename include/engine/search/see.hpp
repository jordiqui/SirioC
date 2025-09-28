#pragma once

#include "engine/types.hpp"

namespace engine {

class Board;

int static_exchange_eval(const Board& board, Move move);

} // namespace engine
