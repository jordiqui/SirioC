#include <algorithm>
#include <bit>
#include <cassert>
#include <iostream>
#include <string>

#include "sirio/board.hpp"
#include "sirio/draws.hpp"
#include "sirio/endgame.hpp"
#include "sirio/move.hpp"
#include "sirio/movegen.hpp"
#include "sirio/evaluation.hpp"
#include "sirio/syzygy.hpp"

void run_perft_tests();

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

void test_bishop_pair_detection() {
    sirio::Board board;
    assert(board.has_bishop_pair(sirio::Color::White));
    assert(board.has_bishop_pair(sirio::Color::Black));

    sirio::Board same_color_bishops{"8/8/8/8/2B5/8/4B3/8 w - - 0 1"};
    assert(!same_color_bishops.has_bishop_pair(sirio::Color::White));

    sirio::Board mixed_bishops{"8/8/8/8/2B5/4B3/8/8 w - - 0 1"};
    assert(mixed_bishops.has_bishop_pair(sirio::Color::White));
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

void test_game_history_tracking() {
    sirio::Board board;
    assert(!board.history().empty());
    assert(board.history().size() == 1);
    assert(board.history().back().zobrist_hash == board.zobrist_hash());

    auto move = sirio::move_from_uci(board, "e2e4");
    sirio::Board after_move = board.apply_move(move);
    assert(after_move.history().size() == 2);
    assert(after_move.history().back().zobrist_hash == after_move.zobrist_hash());

    // Original board history should remain unchanged after applying a move to a copy.
    assert(board.history().size() == 1);
}

void test_null_move() {
    sirio::Board initial{"8/8/8/8/8/8/4P3/4K2k w - - 4 15"};
    sirio::Board after_null = initial.apply_null_move();
    assert(after_null.side_to_move() == sirio::Color::Black);
    assert(!after_null.en_passant_square().has_value());
    assert(after_null.halfmove_clock() == initial.halfmove_clock() + 1);
    assert(after_null.fullmove_number() == initial.fullmove_number());

    sirio::Board after_second_null = after_null.apply_null_move();
    assert(after_second_null.side_to_move() == sirio::Color::White);
    assert(after_second_null.fullmove_number() == after_null.fullmove_number() + 1);
}

void test_evaluation_passed_pawn() {
    sirio::Board passed{"8/8/8/3P4/8/8/8/3kK3 w - - 0 1"};
    sirio::Board blocked{"8/8/3p4/3P4/8/8/8/3kK3 w - - 0 1"};
    int passed_score = sirio::evaluate(passed);
    int blocked_score = sirio::evaluate(blocked);
    assert(passed_score > blocked_score);
}

void test_syzygy_option_configuration() {
    sirio::syzygy::set_tablebase_path("");
    assert(!sirio::syzygy::available());
}

void test_sufficient_material_to_force_checkmate() {
    sirio::Board only_kings{"7k/8/8/8/8/8/8/4K3 w - - 0 1"};
    assert(!sirio::sufficient_material_to_force_checkmate(only_kings));

    sirio::Board with_queen{"7k/8/8/8/8/8/8/4K2Q w - - 0 1"};
    assert(sirio::sufficient_material_to_force_checkmate(with_queen));

    sirio::Board bishop_pair{"7k/8/8/8/8/8/4B3/2B2K2 w - - 0 1"};
    assert(sirio::sufficient_material_to_force_checkmate(bishop_pair));

    sirio::Board bishop_and_knight{"7k/8/8/8/8/8/8/2B2NK1 w - - 0 1"};
    assert(sirio::sufficient_material_to_force_checkmate(bishop_and_knight));

    sirio::Board three_knights{"7k/8/8/8/8/8/8/1NNN2K1 w - - 0 1"};
    assert(sirio::sufficient_material_to_force_checkmate(three_knights));

    sirio::Board knight_vs_knight{"7k/8/8/8/8/8/6N1/4K1N1 w - - 0 1"};
    assert(!sirio::sufficient_material_to_force_checkmate(knight_vs_knight));
}

void test_draw_by_fifty_move_rule() {
    sirio::Board not_draw{"8/8/8/8/8/8/8/4K3 w - - 99 1"};
    assert(!sirio::draw_by_fifty_move_rule(not_draw));

    sirio::Board draw{"8/8/8/8/8/8/8/4K3 w - - 100 1"};
    assert(sirio::draw_by_fifty_move_rule(draw));
}

void test_draw_by_repetition_rule() {
    sirio::Board board;
    const std::string moves[] = {"g1f3", "g8f6", "f3g1", "f6g8", "g1f3", "g8f6", "f3g1", "f6g8"};
    for (const auto &uci : moves) {
        auto move = sirio::move_from_uci(board, uci);
        board = board.apply_move(move);
    }

    int repetitions = sirio::draw_by_repetition_rule(board);
    assert(repetitions >= 3);
    assert(sirio::draw_by_threefold_repetition(board));
}

void test_draw_by_insufficient_material_rule() {
    sirio::Board kings_only{"7k/8/8/8/8/8/8/4K3 w - - 0 1"};
    assert(sirio::draw_by_insufficient_material_rule(kings_only));

    sirio::Board bishop_vs_king{"7k/8/8/8/8/8/8/4KB2 w - - 0 1"};
    assert(sirio::draw_by_insufficient_material_rule(bishop_vs_king));

    sirio::Board knights_each{"7k/8/8/8/8/8/6N1/4K1N1 w - - 0 1"};
    assert(!sirio::draw_by_insufficient_material_rule(knights_each));

    sirio::Board bishops_same_color{"7k/8/8/8/8/8/6b1/4K2B w - - 0 1"};
    assert(sirio::draw_by_insufficient_material_rule(bishops_same_color));

    sirio::Board bishops_opposite_color{"7k/8/8/8/8/8/5b2/4K2B w - - 0 1"};
    assert(!sirio::draw_by_insufficient_material_rule(bishops_opposite_color));
}
}

int main() {
    test_start_position();
    test_fen_roundtrip();
    test_attack_detection();
    test_en_passant();
    test_start_position_moves();
    test_piece_list_updates_after_moves();
    test_bishop_pair_detection();
    test_zobrist_hashing();
    test_game_history_tracking();
    test_sufficient_material_to_force_checkmate();
    test_draw_by_fifty_move_rule();
    test_draw_by_repetition_rule();
    test_draw_by_insufficient_material_rule();
    test_null_move();
    test_evaluation_passed_pawn();
    test_syzygy_option_configuration();
    run_perft_tests();
    std::cout << "All tests passed.\n";
    return 0;
}

