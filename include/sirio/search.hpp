#pragma once

#include <cstdint>
#include <optional>

#include "sirio/board.hpp"
#include "sirio/move.hpp"

namespace sirio {

struct SearchLimits {
    int max_depth = 0;
    int move_time = 0;
    int time_left_white = 0;
    int time_left_black = 0;
    int increment_white = 0;
    int increment_black = 0;
    int moves_to_go = 0;
};

struct SearchResult {
    Move best_move;
    int score = 0;
    bool has_move = false;
    int depth_reached = 0;
    bool timed_out = false;
    std::uint64_t nodes = 0;
};

SearchResult search_best_move(const Board &board, const SearchLimits &limits);

}  // namespace sirio

