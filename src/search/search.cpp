#include "engine/search/search.hpp"
#include "engine/core/board.hpp"

namespace engine {

Move Search::find_bestmove(Board& /*b*/, const Limits& /*lim*/) {
    // TODO: after movegen exists, pick a legal move (PV move or first legal).
    return MOVE_NONE;
}

} // namespace engine
