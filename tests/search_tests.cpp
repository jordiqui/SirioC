#include <cassert>

#include "sirio/board.hpp"
#include "sirio/move.hpp"
#include "sirio/search.hpp"

namespace {

void test_defend_against_direct_mate() {
    sirio::set_search_threads(1);
    sirio::Board board{"5rk1/2b2ppp/8/8/7q/4B3/5PPP/5RK1 w - - 0 1"};
    sirio::SearchLimits limits{};
    limits.max_depth = 1;
    auto result = sirio::search_best_move(board, limits);
    assert(result.has_move);
    assert(sirio::move_to_uci(result.best_move) == "g2g3");
    constexpr int mate_fail_threshold = -90000;
    assert(result.score > mate_fail_threshold);
}

}  // namespace

void run_search_tests() {
    test_defend_against_direct_mate();
}
