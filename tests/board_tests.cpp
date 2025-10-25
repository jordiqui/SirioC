#include <algorithm>
#include <bit>
#include <cassert>
#include <iostream>
#include <string>

#include "sirio/board.hpp"
#include "sirio/move.hpp"
#include "sirio/movegen.hpp"

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

    const auto &white_pawns = board.piece_list(sirio::Color::White, sirio::PieceType::Pawn);
    const auto &black_knights = board.piece_list(sirio::Color::Black, sirio::PieceType::Knight);
    assert(white_pawns.size() == 8);
    assert(std::find(white_pawns.begin(), white_pawns.end(), square_index('a', 2)) != white_pawns.end());
    assert(black_knights.size() == 2);
    assert(std::find(black_knights.begin(), black_knights.end(), square_index('g', 8)) != black_knights.end());
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

void test_start_position_moves() {
    sirio::Board board;
    auto moves = sirio::generate_legal_moves(board);
    assert(moves.size() == 20);
}

void test_piece_list_updates_after_moves() {
    sirio::Board board;
    auto move = sirio::move_from_uci(board, "e2e4");
    sirio::Board after_pawn_push = board.apply_move(move);
    const auto &white_pawns = after_pawn_push.piece_list(sirio::Color::White, sirio::PieceType::Pawn);
    assert(std::find(white_pawns.begin(), white_pawns.end(), square_index('e', 4)) != white_pawns.end());
    assert(std::find(white_pawns.begin(), white_pawns.end(), square_index('e', 2)) == white_pawns.end());

    sirio::Board capture_position{"rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2"};
    auto capture_move = sirio::move_from_uci(capture_position, "e4d5");
    sirio::Board after_capture = capture_position.apply_move(capture_move);
    const auto &white_pawns_after_capture =
        after_capture.piece_list(sirio::Color::White, sirio::PieceType::Pawn);
    const auto &black_pawns_after_capture =
        after_capture.piece_list(sirio::Color::Black, sirio::PieceType::Pawn);
    assert(std::find(white_pawns_after_capture.begin(), white_pawns_after_capture.end(),
                     square_index('d', 5)) != white_pawns_after_capture.end());
    assert(std::find(black_pawns_after_capture.begin(), black_pawns_after_capture.end(),
                     square_index('d', 5)) == black_pawns_after_capture.end());
}

void test_zobrist_hashing() {
    sirio::Board initial;
    sirio::Board another_initial;
    assert(initial.zobrist_hash() == another_initial.zobrist_hash());

    sirio::Board black_to_move{
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1"};
    assert(initial.zobrist_hash() != black_to_move.zobrist_hash());

    auto move = sirio::move_from_uci(initial, "e2e4");
    sirio::Board after_move = initial.apply_move(move);
    sirio::Board reconstructed{after_move.to_fen()};
    assert(after_move.zobrist_hash() == reconstructed.zobrist_hash());

    sirio::Board en_passant_board{
        "rnbqkbnr/pppp1ppp/8/4p3/3P4/8/PPP1PPPP/RNBQKBNR b KQkq d3 0 2"};
    sirio::Board without_en_passant{
        "rnbqkbnr/pppp1ppp/8/4p3/3P4/8/PPP1PPPP/RNBQKBNR b KQkq - 0 2"};
    assert(en_passant_board.zobrist_hash() != without_en_passant.zobrist_hash());
}
}

int main() {
    test_start_position();
    test_fen_roundtrip();
    test_attack_detection();
    test_en_passant();
    test_start_position_moves();
    test_piece_list_updates_after_moves();
    test_zobrist_hashing();
    std::cout << "All tests passed.\n";
    return 0;
}

