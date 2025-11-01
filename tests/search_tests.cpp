#include <cassert>
#include <string>

#include "sirio/board.hpp"
#include "sirio/move.hpp"
#include "sirio/movegen.hpp"
#include "sirio/search.hpp"

namespace sirio {
bool creates_delayed_capture_threat_for_tests(const Board &, const Move &, Color);
bool is_central_pawn_sacrifice_for_tests(const Board &, const Move &, Color);
bool responds_to_direct_threat_for_tests(const Board &, const Move &, Color, bool);
int static_exchange_eval_for_tests(const Board &, const Move &);
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

void test_static_exchange_positive_capture() {
    const std::string fen = "8/8/8/3p4/4P3/8/8/8 w - - 0 1";
    sirio::Board board{fen};
    sirio::Move move = sirio::move_from_uci(board, "e4d5");
    int see_score = sirio::static_exchange_eval_for_tests(board, move);
    assert(see_score >= 100);
}

void test_static_exchange_losing_capture() {
    const std::string fen = "8/8/4q3/3p4/2B5/8/8/4k2K w - - 0 1";
    sirio::Board board{fen};
    sirio::Move move = sirio::move_from_uci(board, "c4d5");
    int see_score = sirio::static_exchange_eval_for_tests(board, move);
    assert(see_score < 0);
}

void test_autoplayer_short_match() {
    sirio::Board board;
    sirio::SearchLimits limits;
    limits.max_depth = 2;
    sirio::set_search_threads(1);
    for (int ply = 0; ply < 6; ++ply) {
        auto result = sirio::search_best_move(board, limits);
        assert(result.has_move);
        board = board.apply_move(result.best_move);
        auto moves = sirio::generate_legal_moves(board);
        if (moves.empty()) {
            break;
        }
    }
}

}  // namespace

void run_search_tests() {
    test_delayed_capture_threat_detection();
    test_central_pawn_sacrifice_detection();
    test_direct_threat_response_detection();
    test_static_exchange_positive_capture();
    test_static_exchange_losing_capture();
    test_autoplayer_short_match();
}
