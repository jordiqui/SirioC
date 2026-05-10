#include <algorithm>
#include <array>
#include <cassert>
#include <optional>
#include <string>
#include <vector>

#include "sirio/board.hpp"
#include "sirio/move.hpp"
#include "sirio/movegen.hpp"

namespace sirio {
std::vector<Move> move_picker_order_snapshot_for_tests(const Board &board, int ply,
                                                       const std::optional<Move> &tt_move,
                                                       bool tactical_only,
                                                       const std::optional<Move> &killer0,
                                                       const std::optional<Move> &killer1);
}

namespace {

std::vector<std::string> moves_to_uci(const std::vector<sirio::Move> &moves) {
    std::vector<std::string> result;
    result.reserve(moves.size());
    for (const auto &move : moves) {
        result.push_back(sirio::move_to_uci(move));
    }
    return result;
}

void assert_no_duplicates(const std::vector<std::string> &moves) {
    std::vector<std::string> sorted = moves;
    std::sort(sorted.begin(), sorted.end());
    auto it = std::adjacent_find(sorted.begin(), sorted.end());
    assert(it == sorted.end());
}

void assert_all_legal(const sirio::Board &board, const std::vector<std::string> &ordered) {
    auto legal = sirio::generate_legal_moves(board);
    std::vector<std::string> legal_uci;
    legal_uci.reserve(legal.size());
    for (const auto &move : legal) {
        legal_uci.push_back(sirio::move_to_uci(move));
    }
    std::sort(legal_uci.begin(), legal_uci.end());
    for (const auto &uci : ordered) {
        assert(std::binary_search(legal_uci.begin(), legal_uci.end(), uci));
    }
}

void run_snapshot_case(const std::string &fen, const std::vector<std::string> &expected, int ply = 0,
                       const std::optional<std::string> &tt_move_uci = std::nullopt,
                       bool tactical_only = false) {
    sirio::Board board{fen};

    std::optional<sirio::Move> tt_move;
    if (tt_move_uci.has_value()) {
        tt_move = sirio::move_from_uci(board, *tt_move_uci);
    }

    auto ordered1 = sirio::move_picker_order_snapshot_for_tests(board, ply, tt_move, tactical_only,
                                                                std::nullopt, std::nullopt);
    auto ordered2 = sirio::move_picker_order_snapshot_for_tests(board, ply, tt_move, tactical_only,
                                                                std::nullopt, std::nullopt);
    auto uci1 = moves_to_uci(ordered1);
    auto uci2 = moves_to_uci(ordered2);

    assert(uci1 == uci2);
    assert(uci1.size() == expected.size());
    assert(!uci1.empty());
    assert(uci1.front() == expected.front());
    assert(uci1 == expected);
    assert_no_duplicates(uci1);
    assert_all_legal(board, uci1);
}

void test_start_position_snapshot() {
    run_snapshot_case(
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        {"a2a3", "a2a4", "b1a3", "b1c3", "b2b3", "b2b4", "c2c3", "c2c4", "d2d3", "d2d4",
         "e2e3", "e2e4", "f2f3", "f2f4", "g1f3", "g1h3", "g2g3", "g2g4", "h2h3", "h2h4"});
}

void test_capture_rich_snapshot() {
    run_snapshot_case("4k3/8/3q4/3p4/3P4/4N3/8/4K3 w - - 0 1",
                      {"e3d5", "e3d1", "e3f1", "e3g4", "e3c2", "e3g2", "e1d1", "e1f1", "e1d2",
                       "e1e2", "e1f2", "d4d5", "d4d6"});
}

void test_quiet_middlegame_snapshot() {
    const std::string fen = "r2q1rk1/pp2bppp/2n1pn2/2bp4/2P5/2NP1NP1/PP2PPBP/R1BQ1RK1 w - - 0 1";
    sirio::Board board{fen};
    auto ordered1 = sirio::move_picker_order_snapshot_for_tests(board, 0, std::nullopt, false,
                                                                std::nullopt, std::nullopt);
    auto ordered2 = sirio::move_picker_order_snapshot_for_tests(board, 0, std::nullopt, false,
                                                                std::nullopt, std::nullopt);
    auto uci1 = moves_to_uci(ordered1);
    auto uci2 = moves_to_uci(ordered2);
    assert(uci1 == uci2);
    assert(!uci1.empty());
    assert_no_duplicates(uci1);
    assert_all_legal(board, uci1);
}

void test_promotion_snapshot() {
    run_snapshot_case("4k3/6P1/8/8/8/8/8/4K3 w - - 0 1",
                      {"g7g8q", "g7g8r", "g7g8b", "g7g8n", "e1d1", "e1f1", "e1d2", "e1e2", "e1f2"});
}

void test_tt_move_priority_snapshot() {
    run_snapshot_case("8/8/3k4/3p4/3P4/8/3K4/8 w - - 0 1",
                      {"d2e3", "d4d5", "d2c1", "d2d1", "d2e1", "d2c2", "d2e2", "d2c3", "d2d3"}, 0,
                      std::optional<std::string>{"d2e3"});
}

}  // namespace

void run_move_picker_snapshot_tests() {
    test_start_position_snapshot();
    test_capture_rich_snapshot();
    test_quiet_middlegame_snapshot();
    test_promotion_snapshot();
    test_tt_move_priority_snapshot();
}
