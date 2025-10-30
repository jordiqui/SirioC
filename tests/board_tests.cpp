#include <algorithm>
#include <filesystem>
#include <fstream>
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
void run_tt_tests();
void run_evaluation_phase_tests();
void run_search_tests();

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
    const std::string fen = "8/8/8/3Pp3/8/8/8/4K3 w - e6 0 1";
    sirio::Board board{fen};
    auto ep = board.en_passant_square();
    assert(ep.has_value());
    assert(*ep == square_index('e', 6));
    assert(board.to_fen() == fen);
}

void test_en_passant_zobrist_hash_without_capture() {
    const std::string with_ep = "4k3/8/8/8/P7/8/8/4K3 b - a3 0 1";
    const std::string without_ep = "4k3/8/8/8/P7/8/8/4K3 b - - 0 1";
    sirio::Board board_with{with_ep};
    sirio::Board board_without{without_ep};
    assert(board_with.zobrist_hash() == board_without.zobrist_hash());
    assert(!board_with.en_passant_square().has_value());
    assert(board_with.to_fen() == without_ep);
}

void test_en_passant_zobrist_hash_with_capture() {
    const std::string with_ep = "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1";
    const std::string without_ep = "4k3/8/8/3pP3/8/8/8/4K3 w - - 0 1";
    sirio::Board board_with{with_ep};
    sirio::Board board_without{without_ep};
    assert(board_with.zobrist_hash() != board_without.zobrist_hash());
}

void test_start_position_moves() {
    sirio::Board board;
    auto moves = sirio::generate_legal_moves(board);
    assert(moves.size() == 20);
}

void test_en_passant_requires_available_capture() {
    sirio::Board initial;
    sirio::Move double_push = sirio::move_from_uci(initial, "a2a4");
    sirio::Board after_double_push = initial.apply_move(double_push);
    assert(!after_double_push.en_passant_square().has_value());
    assert(after_double_push.to_fen() ==
           "rnbqkbnr/pppppppp/8/8/P7/8/1PPPPPPP/RNBQKBNR b KQkq - 0 1");
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

    sirio::Board en_passant_board{"4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1"};
    sirio::Board without_en_passant{"4k3/8/8/3pP3/8/8/8/4K3 w - - 0 1"};
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

void test_apply_uci_move_handles_null_and_invalid_tokens() {
    sirio::Board board;

    // Null moves should be accepted and toggle the side to move.
    assert(sirio::apply_uci_move(board, "0000"));
    assert(board.side_to_move() == sirio::Color::Black);

    assert(sirio::apply_uci_move(board, "0000"));
    assert(board.side_to_move() == sirio::Color::White);

    // Legal moves should still be applied normally.
    assert(sirio::apply_uci_move(board, "e2e4"));
    assert(board.side_to_move() == sirio::Color::Black);

    const std::string before_invalid = board.to_fen();
    assert(!sirio::apply_uci_move(board, "zzzz"));
    assert(board.to_fen() == before_invalid);
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

void test_king_safety_advanced_enemy_pawns() {
    sirio::use_classical_evaluation();

    sirio::Board distant{"6k1/8/5p2/8/8/8/8/6K1 w - - 0 1"};
    sirio::initialize_evaluation(distant);
    int distant_eval = sirio::evaluate(distant);

    sirio::Board close{"6k1/8/8/8/8/5p2/8/6K1 w - - 0 1"};
    sirio::initialize_evaluation(close);
    int close_eval = sirio::evaluate(close);

    assert(close_eval < distant_eval);
}

void test_king_safety_heavy_piece_alignment() {
    sirio::use_classical_evaluation();

    sirio::Board aligned{"6rk/8/8/8/8/8/8/6K1 w - - 0 1"};
    sirio::initialize_evaluation(aligned);
    int aligned_eval = sirio::evaluate(aligned);

    sirio::Board displaced{"r5k1/8/8/8/8/8/8/6K1 w - - 0 1"};
    sirio::initialize_evaluation(displaced);
    int displaced_eval = sirio::evaluate(displaced);

    assert(aligned_eval < displaced_eval);
}

void test_king_safety_defender_support() {
    sirio::use_classical_evaluation();

    sirio::Board defended{"6k1/8/8/8/8/8/8/5RK1 w - - 0 1"};
    sirio::initialize_evaluation(defended);
    int defended_eval = sirio::evaluate(defended);

    sirio::Board distant{"6k1/8/8/8/8/8/8/R5K1 w - - 0 1"};
    sirio::initialize_evaluation(distant);
    int distant_eval = sirio::evaluate(distant);

    assert(defended_eval > distant_eval);
}

void test_king_safety_weak_squares() {
    sirio::use_classical_evaluation();

    sirio::Board exposed{"6k1/8/8/8/8/6q1/8/6K1 w - - 0 1"};
    sirio::initialize_evaluation(exposed);
    int exposed_eval = sirio::evaluate(exposed);

    sirio::Board distant_threat{"6k1/q7/8/8/8/8/8/6K1 w - - 0 1"};
    sirio::initialize_evaluation(distant_threat);
    int distant_eval = sirio::evaluate(distant_threat);

    assert(exposed_eval < distant_eval);
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
void test_evaluation_backend_consistency() {
    sirio::Board board;
    sirio::use_classical_evaluation();
    sirio::initialize_evaluation(board);

    int initial_eval = sirio::evaluate(board);

    sirio::Move move = sirio::move_from_uci(board, "e2e4");
    sirio::Board after = board.apply_move(move);
    int stacked_eval = sirio::evaluate(after);

    sirio::pop_evaluation_state();
    sirio::initialize_evaluation(after);
    int fresh_eval = sirio::evaluate(after);
    assert(stacked_eval == fresh_eval);

    sirio::initialize_evaluation(board);
    int restored_eval = sirio::evaluate(board);
    assert(initial_eval == restored_eval);
}

void test_nnue_backend_material_weights() {
    namespace fs = std::filesystem;
    const fs::path temp_path = fs::temp_directory_path() / "sirio_test.nnue";
    {
        std::ofstream output(temp_path);
        output << "SirioNNUE1\n";
        output << "0 100\n";
        const double weights[] = {1.0, 3.0, 3.0, 5.0, 9.0, 0.0, -1.0, -3.0, -3.0, -5.0, -9.0, 0.0};
        for (double weight : weights) {
            output << weight << ' ';
        }
        output << '\n';
    }

    std::string error;
    auto backend = sirio::make_nnue_evaluation(temp_path.string(), &error);
    assert(backend);
    assert(error.empty());
    sirio::set_evaluation_backend(std::move(backend));

    sirio::Board equal;
    sirio::initialize_evaluation(equal);
    int equal_eval = sirio::evaluate(equal);
    assert(equal_eval == 0);

    sirio::Board advantage{"rnbqkbnr/1ppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
    sirio::initialize_evaluation(advantage);
    int advantage_eval = sirio::evaluate(advantage);
    assert(advantage_eval > 0);

    sirio::use_classical_evaluation();
    sirio::initialize_evaluation(equal);
    std::error_code ec;
    fs::remove(temp_path, ec);
}

}

int main() {
    test_start_position();
    test_fen_roundtrip();
    test_attack_detection();
    test_en_passant();
    test_en_passant_zobrist_hash_without_capture();
    test_en_passant_zobrist_hash_with_capture();
    test_start_position_moves();
    test_en_passant_requires_available_capture();
    test_piece_list_updates_after_moves();
    test_bishop_pair_detection();
    test_zobrist_hashing();
    test_game_history_tracking();
    test_apply_uci_move_handles_null_and_invalid_tokens();
    test_sufficient_material_to_force_checkmate();
    test_draw_by_fifty_move_rule();
    test_draw_by_repetition_rule();
    test_draw_by_insufficient_material_rule();
    test_null_move();
    test_king_safety_advanced_enemy_pawns();
    test_king_safety_heavy_piece_alignment();
    test_king_safety_defender_support();
    test_king_safety_weak_squares();
    test_evaluation_passed_pawn();
    test_syzygy_option_configuration();
    test_evaluation_backend_consistency();
    test_nnue_backend_material_weights();
    run_evaluation_phase_tests();
    run_search_tests();
    run_tt_tests();
    run_perft_tests();
    std::cout << "All tests passed.\n";
    return 0;
}

