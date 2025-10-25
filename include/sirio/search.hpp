#pragma once

#include <optional>

#include "sirio/board.hpp"
#include "sirio/move.hpp"

namespace sirio {

struct SearchResult {
    Move best_move;
    int score = 0;
    bool has_move = false;
};

SearchResult search_best_move(const Board &board, int depth);

}  // namespace sirio

