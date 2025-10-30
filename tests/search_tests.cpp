#include <cassert>
#include <string>

#include "sirio/board.hpp"
#include "sirio/move.hpp"
#include "sirio/search.hpp"

namespace sirio {
bool creates_delayed_capture_threat_for_tests(const Board &, const Move &, Color);
bool is_central_pawn_sacrifice_for_tests(const Board &, const Move &, Color);
bool responds_to_direct_threat_for_tests(const Board &, const Move &, Color, bool);
}  // namespace sirio

namespace {

void test_delayed_capture_threat_detection() {
    const std::string fen = "6k1/8/8/7r/8/8/8/3Q2K1 w - - 0 1";
    sirio::Board board{fen};
    sirio::Move move = sirio::move_from_uci(board, "d1g4");
    sirio::Board after = board.apply_move(move);
    assert(sirio::creates_delayed_capture_threat_for_tests(after, move, sirio::Color::White));
}

void test_central_pawn_sacrifice_detection() {
    const std::string fen = "6k1/2b2npp/8/8/4P3/8/6PP/6K1 w - - 0 1";
    sirio::Board board{fen};
    sirio::Move move = sirio::move_from_uci(board, "e4e5");
    sirio::Board after = board.apply_move(move);
    assert(sirio::is_central_pawn_sacrifice_for_tests(after, move, sirio::Color::White));
}

void test_direct_threat_response_detection() {
    const std::string fen = "6k1/8/8/2b5/8/5N2/6PP/6K1 w - - 0 1";
    sirio::Board board{fen};
    sirio::Move move = sirio::move_from_uci(board, "g1f1");
    assert(sirio::responds_to_direct_threat_for_tests(board, move, sirio::Color::White, board.in_check(sirio::Color::White)));
}

}  // namespace

void run_search_tests() {
    test_delayed_capture_threat_detection();
    test_central_pawn_sacrifice_detection();
    test_direct_threat_response_detection();
}
