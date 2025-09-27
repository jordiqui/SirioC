#pragma once
#include "engine/types.hpp"

namespace engine {
class Board;

class Search {
public:
    Search() = default;
    Move find_bestmove(Board& b, const Limits& lim);

private:
    // TODO: transposition table, history, time manager
};

} // namespace engine
