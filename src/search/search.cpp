#include "engine/search/search.hpp"
#include "engine/core/board.hpp"

namespace engine {

Move Search::find_bestmove(Board& b, const Limits& /*lim*/) {
    auto legal = b.generate_legal_moves();
    if (legal.empty()) return MOVE_NONE;
    // Placeholder: return the first legal move.
    return legal.front();
}

} // namespace engine
