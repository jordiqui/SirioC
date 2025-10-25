#include <bit>
#include <cassert>
#include <iostream>
#include <string>

#include "sirio/board.hpp"

namespace {
int square_index(char file, int rank) {
    return (rank - 1) * 8 + (file - 'a');
}

void test_start_position() {
    sirio::Board board;
    const auto white_occ = board.occupancy(sirio::Color::White);
    const auto black_occ = board.occupancy(sirio::Color::Black);
    assert(std::popcount(white_occ) == 16);
    assert(std::popcount(black_occ) == 16);
    assert(board.side_to_move() == sirio::Color::White);
    assert(board.castling_rights().white_kingside);
    assert(board.castling_rights().white_queenside);
    assert(board.castling_rights().black_kingside);
    assert(board.castling_rights().black_queenside);
    assert(board.to_fen() ==
           "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void test_fen_roundtrip() {
    const std::string fen = "8/8/8/3k4/4R3/8/8/4K3 w - - 1 42";
    sirio::Board board{fen};
    assert(board.to_fen() == fen);
    assert(board.halfmove_clock() == 1);
    assert(board.fullmove_number() == 42);
}

void test_attack_detection() {
    const std::string fen = "8/8/8/3k4/4R3/8/8/4K3 w - - 0 1";
    sirio::Board board{fen};
    const int e5 = square_index('e', 5);
    const int d5 = square_index('d', 5);
    assert(board.is_square_attacked(e5, sirio::Color::White));
    assert(!board.is_square_attacked(d5, sirio::Color::White));
    const std::string knight_fen = "8/8/8/3k4/2N5/8/8/4K3 w - - 0 1";
    sirio::Board knight_board{knight_fen};
    const int d6 = square_index('d', 6);
    const int e4 = square_index('e', 4);
    assert(knight_board.is_square_attacked(d6, sirio::Color::White));
    assert(!knight_board.is_square_attacked(e4, sirio::Color::White));
}

void test_en_passant() {
    const std::string fen = "rnbqkbnr/pppppppp/8/8/3P4/8/PPP1PPPP/RNBQKBNR b KQkq d3 0 1";
    sirio::Board board{fen};
    auto ep = board.en_passant_square();
    assert(ep.has_value());
    assert(*ep == square_index('d', 3));
}
}

int main() {
    test_start_position();
    test_fen_roundtrip();
    test_attack_detection();
    test_en_passant();
    std::cout << "All tests passed.\n";
    return 0;
}

