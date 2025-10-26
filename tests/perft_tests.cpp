#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "sirio/board.hpp"
#include "sirio/move.hpp"
#include "sirio/movegen.hpp"

namespace {

std::uint64_t perft(const sirio::Board &board, int depth) {
    if (depth == 0) {
        return 1;
    }

    auto moves = sirio::generate_legal_moves(board);
    if (depth == 1) {
        return static_cast<std::uint64_t>(moves.size());
    }

    std::uint64_t nodes = 0;
    for (const auto &move : moves) {
        sirio::Board next = board.apply_move(move);
        nodes += perft(next, depth - 1);
    }
    return nodes;
}

void test_start_position_perft() {
    sirio::Board board;
    assert(perft(board, 1) == 20);
    assert(perft(board, 2) == 400);
}

void test_kiwipete_position() {
    sirio::Board board;
    board.set_from_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    assert(perft(board, 1) == 48);
    assert(perft(board, 2) == 2039);
    assert(perft(board, 3) == 97862);
}

void test_en_passant_perft() {
    sirio::Board board;
    board.set_from_fen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    assert(perft(board, 1) == 14);
    assert(perft(board, 2) == 191);
    assert(perft(board, 3) == 2812);
}

void test_promotion_and_castling_perft() {
    sirio::Board board;
    board.set_from_fen("r3k2r/Pppppppp/8/8/8/8/pppppppp/R3K2R w KQkq - 0 1");
    assert(perft(board, 1) == 3);
    assert(perft(board, 2) == 156);
    assert(perft(board, 3) == 2391);
}

}  // namespace

void run_perft_tests() {
    test_start_position_perft();
    test_kiwipete_position();
    test_en_passant_perft();
    test_promotion_and_castling_perft();
}

