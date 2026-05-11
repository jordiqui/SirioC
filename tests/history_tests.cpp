#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "sirio/board.hpp"
#include "sirio/history.hpp"
#include "sirio/move.hpp"
#include "sirio/search_params.hpp"

namespace {

void test_initial_state_neutral_and_empty_killers() {
    sirio::SearchHistory history;
    sirio::Board start;
    const sirio::Move quiet = sirio::move_from_uci(start, "e2e4");

    assert(history.quiet_history_score(quiet, sirio::Color::White) == 0);
    assert(history.quiet_history_score(quiet, sirio::Color::Black) == 0);

    const auto &killers = history.killer_slots(0);
    assert(!killers[0].has_value());
    assert(!killers[1].has_value());
}

void test_is_quiet_move_predicate() {
    sirio::Board quiet_board;
    const sirio::Move quiet = sirio::move_from_uci(quiet_board, "g1f3");
    assert(sirio::is_quiet_move(quiet));

    sirio::Board capture_board{"8/8/8/3p4/4P3/8/8/8 w - - 0 1"};
    const sirio::Move capture = sirio::move_from_uci(capture_board, "e4d5");
    assert(!sirio::is_quiet_move(capture));
}

void test_quiet_history_update_bonus_clamp_and_saturation() {
    sirio::SearchHistory history;
    sirio::Board start;
    const sirio::Move quiet = sirio::move_from_uci(start, "e2e4");

    history.update_quiet_history(sirio::Color::White, quiet, 3, true);
    assert(history.quiet_history_score(quiet, sirio::Color::White) == 9);

    history.update_quiet_history(sirio::Color::White, quiet, 10'000, true);
    const int expected_after_clamped_bonus =
        9 + sirio::search_params::history_bonus_limit;
    assert(history.quiet_history_score(quiet, sirio::Color::White) == expected_after_clamped_bonus);

    for (int i = 0; i < 2000; ++i) {
        history.update_quiet_history(sirio::Color::White, quiet, 1000, true);
    }
    assert(history.quiet_history_score(quiet, sirio::Color::White) == sirio::search_params::history_max);

    for (int i = 0; i < 5000; ++i) {
        history.update_quiet_history(sirio::Color::White, quiet, 1000, false);
    }
    assert(history.quiet_history_score(quiet, sirio::Color::White) == sirio::search_params::history_min);
}

void test_store_killer_slots_and_duplicate_handling() {
    sirio::SearchHistory history;
    sirio::Board start;
    const sirio::Move first = sirio::move_from_uci(start, "e2e4");
    const sirio::Move second = sirio::move_from_uci(start, "d2d4");

    history.store_killer(first, 4);
    auto killers = history.killer_slots(4);
    assert(killers[0].has_value());
    assert(killers[0]->from == first.from && killers[0]->to == first.to);
    assert(!killers[1].has_value());

    history.store_killer(second, 4);
    killers = history.killer_slots(4);
    assert(killers[0].has_value());
    assert(killers[0]->from == second.from && killers[0]->to == second.to);
    assert(killers[1].has_value());
    assert(killers[1]->from == first.from && killers[1]->to == first.to);

    history.store_killer(second, 4);
    killers = history.killer_slots(4);
    assert(killers[0].has_value());
    assert(killers[1].has_value());
    assert(killers[0]->from == second.from && killers[0]->to == second.to);
    assert(killers[1]->from == first.from && killers[1]->to == first.to);
}

void test_isolation_between_entries() {
    sirio::SearchHistory history;
    sirio::Board start;
    const sirio::Move e2e4 = sirio::move_from_uci(start, "e2e4");
    const sirio::Move d2d4 = sirio::move_from_uci(start, "d2d4");

    history.update_quiet_history(sirio::Color::White, e2e4, 4, true);
    assert(history.quiet_history_score(e2e4, sirio::Color::White) > 0);
    assert(history.quiet_history_score(d2d4, sirio::Color::White) == 0);
    assert(history.quiet_history_score(e2e4, sirio::Color::Black) == 0);

    history.store_killer(e2e4, 2);
    const auto &ply2 = history.killer_slots(2);
    const auto &ply3 = history.killer_slots(3);
    assert(ply2[0].has_value());
    assert(!ply3[0].has_value());
    assert(!ply3[1].has_value());
}

void test_capture_history_scaffold_basics() {
    sirio::SearchHistory history;
    sirio::Board capture_board{"8/8/8/3p4/4P3/8/8/8 w - - 0 1"};
    const sirio::Move e4d5 = sirio::move_from_uci(capture_board, "e4d5");

    assert(history.capture_history().score(e4d5, sirio::Color::White) == 0);
    history.capture_history().update(sirio::Color::White, e4d5, 3, true);
    assert(history.capture_history().score(e4d5, sirio::Color::White) == 9);
    history.capture_history().update(sirio::Color::White, e4d5, 3, false);
    assert(history.capture_history().score(e4d5, sirio::Color::White) == 0);
}

void test_capture_history_clamp_and_indexing() {
    sirio::SearchHistory history;
    sirio::Board board_a{"8/8/8/3p4/4P3/8/8/8 w - - 0 1"};
    sirio::Board board_b{"8/8/8/3n4/4P3/8/8/8 w - - 0 1"};
    const sirio::Move pawn_takes_pawn = sirio::move_from_uci(board_a, "e4d5");
    const sirio::Move pawn_takes_knight = sirio::move_from_uci(board_b, "e4d5");

    history.capture_history().update(sirio::Color::White, pawn_takes_pawn, 1000, true);
    assert(history.capture_history().score(pawn_takes_pawn, sirio::Color::White) ==
           sirio::search_params::history_bonus_limit);
    assert(history.capture_history().score(pawn_takes_knight, sirio::Color::White) == 0);
    assert(history.capture_history().score(pawn_takes_pawn, sirio::Color::Black) == 0);

    for (int i = 0; i < 5000; ++i) {
        history.capture_history().update(sirio::Color::White, pawn_takes_pawn, 1000, true);
    }
    assert(history.capture_history().score(pawn_takes_pawn, sirio::Color::White) ==
           sirio::search_params::history_max);
}

void test_noisy_history_scaffold_basics_and_determinism() {
    sirio::SearchHistory history;
    sirio::Board promotion_board{"4k3/6P1/8/8/8/8/8/4K3 w - - 0 1"};
    const sirio::Move g7g8q = sirio::move_from_uci(promotion_board, "g7g8q");
    const sirio::Move g7g8r = sirio::move_from_uci(promotion_board, "g7g8r");

    assert(history.noisy_history().score(g7g8q, sirio::Color::White) == 0);
    history.noisy_history().update(sirio::Color::White, g7g8q, 2, true);
    assert(history.noisy_history().score(g7g8q, sirio::Color::White) == 4);
    assert(history.noisy_history().score(g7g8r, sirio::Color::White) == 4);

    history.noisy_history().update(sirio::Color::White, g7g8q, 2, false);
    assert(history.noisy_history().score(g7g8q, sirio::Color::White) == 0);
    assert(history.noisy_history().score(g7g8r, sirio::Color::White) == 0);
}

void test_history_clear_resets_quiet_killer_capture_and_noisy() {
    sirio::SearchHistory history;
    sirio::Board start;
    sirio::Board capture_board{"8/8/8/3p4/4P3/8/8/8 w - - 0 1"};
    sirio::Board promotion_board{"4k3/6P1/8/8/8/8/8/4K3 w - - 0 1"};
    const sirio::Move quiet = sirio::move_from_uci(start, "e2e4");
    const sirio::Move capture = sirio::move_from_uci(capture_board, "e4d5");
    const sirio::Move noisy = sirio::move_from_uci(promotion_board, "g7g8q");

    history.update_quiet_history(sirio::Color::White, quiet, 3, true);
    history.store_killer(quiet, 1);
    history.capture_history().update(sirio::Color::White, capture, 3, true);
    history.noisy_history().update(sirio::Color::White, noisy, 3, true);
    history.clear();

    assert(history.quiet_history_score(quiet, sirio::Color::White) == 0);
    assert(!history.killer_slots(1)[0].has_value());
    assert(history.capture_history().score(capture, sirio::Color::White) == 0);
    assert(history.noisy_history().score(noisy, sirio::Color::White) == 0);
}

void test_continuation_history_default_update_and_clear() {
    sirio::SearchHistory history;
    sirio::Board start;
    const sirio::Move e2e4 = sirio::move_from_uci(start, "e2e4");
    const sirio::Move g1f3 = sirio::move_from_uci(start, "g1f3");

    assert(history.continuation_history().score(sirio::Color::White, e2e4, sirio::Color::White, g1f3) == 0);

    history.continuation_history().update(sirio::Color::White, e2e4, sirio::Color::White, g1f3, 3, true);
    assert(history.continuation_history().score(sirio::Color::White, e2e4, sirio::Color::White, g1f3) == 9);

    history.continuation_history().update(sirio::Color::White, e2e4, sirio::Color::White, g1f3, 3, false);
    assert(history.continuation_history().score(sirio::Color::White, e2e4, sirio::Color::White, g1f3) == 0);

    history.continuation_history().clear();
    assert(history.continuation_history().score(sirio::Color::White, e2e4, sirio::Color::White, g1f3) == 0);
}

void test_continuation_history_clamp_indexing_and_determinism() {
    sirio::SearchHistory history;
    sirio::Board start;
    const sirio::Move e2e4 = sirio::move_from_uci(start, "e2e4");
    const sirio::Move g1f3 = sirio::move_from_uci(start, "g1f3");
    const sirio::Move d2d4 = sirio::move_from_uci(start, "d2d4");

    for (int i = 0; i < 5000; ++i) {
        history.continuation_history().update(sirio::Color::White, e2e4, sirio::Color::White, g1f3, 1000, true);
    }
    assert(history.continuation_history().score(sirio::Color::White, e2e4, sirio::Color::White, g1f3) ==
           sirio::search_params::history_max);

    for (int i = 0; i < 10000; ++i) {
        history.continuation_history().update(sirio::Color::White, e2e4, sirio::Color::White, g1f3, 1000, false);
    }
    assert(history.continuation_history().score(sirio::Color::White, e2e4, sirio::Color::White, g1f3) ==
           sirio::search_params::history_min);

    assert(history.continuation_history().score(sirio::Color::White, e2e4, sirio::Color::White, d2d4) == 0);
    assert(history.continuation_history().score(sirio::Color::White, d2d4, sirio::Color::White, g1f3) == 0);
    assert(history.continuation_history().score(sirio::Color::Black, e2e4, sirio::Color::White, g1f3) == 0);

    history.continuation_history().update(sirio::Color::White, d2d4, sirio::Color::White, g1f3, 2, true);
    history.continuation_history().update(sirio::Color::White, d2d4, sirio::Color::White, g1f3, 2, true);
    assert(history.continuation_history().score(sirio::Color::White, d2d4, sirio::Color::White, g1f3) == 8);
}


void test_correction_history_default_update_clamp_and_clear() {
    sirio::SearchHistory history;
    constexpr std::size_t bucket = 7;

    assert(history.correction_history().score(sirio::Color::White, bucket) == 0);
    assert(history.correction_history().score(sirio::Color::Black, bucket) == 0);

    history.correction_history().update(sirio::Color::White, bucket, 3, true);
    assert(history.correction_history().score(sirio::Color::White, bucket) == 9);

    history.correction_history().update(sirio::Color::White, bucket, 3, false);
    assert(history.correction_history().score(sirio::Color::White, bucket) == 0);

    for (int i = 0; i < 5000; ++i) {
        history.correction_history().update(sirio::Color::White, bucket, 1000, true);
    }
    assert(history.correction_history().score(sirio::Color::White, bucket) == sirio::search_params::correction_history_max);

    for (int i = 0; i < 10000; ++i) {
        history.correction_history().update(sirio::Color::White, bucket, 1000, false);
    }
    assert(history.correction_history().score(sirio::Color::White, bucket) == sirio::search_params::correction_history_min);

    history.correction_history().clear();
    assert(history.correction_history().score(sirio::Color::White, bucket) == 0);
}

void test_correction_history_bucket_indexing_and_determinism() {
    sirio::SearchHistory history;

    constexpr std::size_t bucket_a = 12;
    constexpr std::size_t bucket_b = 13;
    constexpr std::size_t wrapped_bucket = 12 + 1024;

    history.correction_history().update(sirio::Color::White, bucket_a, 2, true);
    history.correction_history().update(sirio::Color::White, bucket_a, 2, true);

    assert(history.correction_history().score(sirio::Color::White, bucket_a) == 8);
    assert(history.correction_history().score(sirio::Color::White, bucket_b) == 0);
    assert(history.correction_history().score(sirio::Color::Black, bucket_a) == 0);
    assert(history.correction_history().score(sirio::Color::White, wrapped_bucket) ==
           history.correction_history().score(sirio::Color::White, bucket_a));
}

void test_search_history_clear_resets_correction_history() {
    sirio::SearchHistory history;
    constexpr std::size_t bucket = 29;

    history.correction_history().update(sirio::Color::White, bucket, 4, true);
    assert(history.correction_history().score(sirio::Color::White, bucket) > 0);

    history.clear();
    assert(history.correction_history().score(sirio::Color::White, bucket) == 0);
}

void test_correction_history_key_extraction_valid_white_and_black() {
    const auto white_key = sirio::make_correction_history_key_for_tests(sirio::Color::White, 17);
    const auto black_key = sirio::make_correction_history_key_for_tests(sirio::Color::Black, 23);
    assert(white_key.has_value());
    assert(black_key.has_value());
    assert(white_key->mover_color == sirio::Color::White);
    assert(white_key->bucket == 17);
    assert(black_key->mover_color == sirio::Color::Black);
    assert(black_key->bucket == 23);
}

void test_correction_history_key_extraction_bucket_normalization_and_aliases() {
    const auto bucket_base = sirio::make_correction_history_key_for_tests(sirio::Color::White, 12);
    const auto bucket_alias = sirio::make_correction_history_key_for_tests(sirio::Color::White, 12 + 1024);
    const auto bucket_distinct = sirio::make_correction_history_key_for_tests(sirio::Color::White, 13);
    assert(bucket_base.has_value());
    assert(bucket_alias.has_value());
    assert(bucket_distinct.has_value());
    assert(bucket_base->bucket == bucket_alias->bucket);
    assert(bucket_base->bucket != bucket_distinct->bucket);
}

void test_correction_history_key_extraction_invalid_color_fails() {
    const auto invalid_color = static_cast<sirio::Color>(99);
    const auto key = sirio::make_correction_history_key_for_tests(invalid_color, 5);
    assert(!key.has_value());
}

void test_correction_history_key_extraction_is_deterministic() {
    const auto first = sirio::make_correction_history_key_for_tests(sirio::Color::Black, 2049);
    const auto second = sirio::make_correction_history_key_for_tests(sirio::Color::Black, 2049);
    assert(first.has_value());
    assert(second.has_value());
    assert(first->mover_color == second->mover_color);
    assert(first->bucket == second->bucket);
}

void test_correction_history_key_targets_expected_bucket_for_update_and_read() {
    sirio::SearchHistory history;
    const auto key = sirio::make_correction_history_key_for_tests(sirio::Color::White, 7 + 1024);
    assert(key.has_value());
    history.correction_history().update(key->mover_color, key->bucket, 2, true);

    assert(history.correction_history().score(sirio::Color::White, 7) == 4);
    assert(history.correction_history().score(sirio::Color::White, 7 + 1024) == 4);
}

void test_correction_history_static_eval_helper_zero_state_returns_raw_eval() {
    sirio::SearchHistory history;
    const auto key = sirio::make_correction_history_key_for_tests(sirio::Color::White, 11);
    assert(key.has_value());
    assert(sirio::apply_correction_history_to_static_eval(42, history, key) == 42);
}

void test_correction_history_static_eval_helper_invalid_key_returns_raw_eval() {
    sirio::SearchHistory history;
    assert(sirio::apply_correction_history_to_static_eval(-17, history, std::nullopt) == -17);
}

void test_correction_history_static_eval_helper_seeded_delta_and_clear_contract() {
    sirio::SearchHistory history;
    const auto key = sirio::make_correction_history_key_for_tests(sirio::Color::White, 5);
    assert(key.has_value());
    const int raw_eval = 100;

    history.correction_history().update(key->mover_color, key->bucket, 3, true);
    const int correction = history.correction_history().score(*key);
    assert(correction == 9);
    assert(sirio::apply_correction_history_to_static_eval(raw_eval, history, key) == raw_eval + correction);

    history.clear();
    assert(sirio::apply_correction_history_to_static_eval(raw_eval, history, key) == raw_eval);
}

void test_correction_history_static_eval_helper_is_read_only() {
    sirio::SearchHistory history;
    const auto key = sirio::make_correction_history_key_for_tests(sirio::Color::Black, 12);
    assert(key.has_value());
    history.correction_history().update(key->mover_color, key->bucket, 2, true);
    const int before = history.correction_history().score(*key);
    assert(sirio::apply_correction_history_to_static_eval(0, history, key) == before);
    assert(history.correction_history().score(*key) == before);
}

void test_correction_history_position_key_same_structure_and_side_is_deterministic() {
    const sirio::Board board_a;
    const sirio::Board board_b;
    const auto key_a = sirio::make_correction_history_key_from_position_for_tests(board_a);
    const auto key_b = sirio::make_correction_history_key_from_position_for_tests(board_b);
    assert(key_a.has_value() && key_b.has_value());
    assert(key_a->mover_color == key_b->mover_color);
    assert(key_a->bucket == key_b->bucket);
}

void test_correction_history_position_key_changes_with_side_to_move() {
    const sirio::Board white_to_move{"8/8/8/3p4/4P3/8/8/4K2k w - - 0 1"};
    const sirio::Board black_to_move{"8/8/8/3p4/4P3/8/8/4K2k b - - 0 1"};
    const auto white_key = sirio::make_correction_history_key_from_position_for_tests(white_to_move);
    const auto black_key = sirio::make_correction_history_key_from_position_for_tests(black_to_move);
    assert(white_key.has_value() && black_key.has_value());
    assert(white_key->bucket == black_key->bucket);
    assert(white_key->mover_color != black_key->mover_color);
}

void test_correction_history_position_key_changes_with_pawn_structure() {
    const sirio::Board pawns_a{"8/8/8/8/4P3/8/8/4K2k w - - 0 1"};
    const sirio::Board pawns_b{"8/8/8/8/3P4/8/8/4K2k w - - 0 1"};
    const auto key_a = sirio::make_correction_history_key_from_position_for_tests(pawns_a);
    const auto key_b = sirio::make_correction_history_key_from_position_for_tests(pawns_b);
    assert(key_a.has_value() && key_b.has_value());
    assert(key_a->mover_color == key_b->mover_color);
    assert(key_a->bucket != key_b->bucket);
}

void test_correction_history_position_key_helper_is_read_only_and_clear_independent() {
    sirio::SearchHistory history;
    const sirio::Board board{"8/8/8/3p4/4P3/8/8/4K2k w - - 0 1"};
    const std::string before_fen = board.to_fen();
    const auto key_before = sirio::make_correction_history_key_from_position_for_tests(board);
    assert(key_before.has_value());
    history.correction_history().update(key_before->mover_color, key_before->bucket, 3, true);
    const int correction_before_clear = history.correction_history().score(*key_before);
    assert(correction_before_clear > 0);
    assert(sirio::apply_correction_history_to_static_eval(10, history, key_before) == 10 + correction_before_clear);

    history.clear();
    const auto key_after = sirio::make_correction_history_key_from_position_for_tests(board);
    assert(key_after.has_value());
    assert(key_before->mover_color == key_after->mover_color);
    assert(key_before->bucket == key_after->bucket);
    assert(board.to_fen() == before_fen);
    assert(history.correction_history().score(*key_after) == 0);
}

void test_noisy_history_key_extraction_for_quiet_move_fails() {
    sirio::Board board;
    const std::string before = board.to_fen();
    const sirio::Move quiet = sirio::move_from_uci(board, "e2e4");

    const auto key = sirio::make_noisy_history_key_for_tests(board, quiet);
    assert(!key.has_value());
    assert(board.to_fen() == before);
}

void test_capture_history_key_extraction_for_capture_succeeds() {
    sirio::Board board{"8/8/8/3p4/4P3/8/8/8 w - - 0 1"};
    const std::string before = board.to_fen();
    const sirio::Move capture = sirio::move_from_uci(board, "e4d5");

    const auto key = sirio::make_capture_history_key_for_tests(board, capture);
    assert(key.has_value());
    assert(key->mover == sirio::Color::White);
    assert(key->attacker == sirio::PieceType::Pawn);
    assert(key->captured == sirio::PieceType::Pawn);
    assert(key->to == capture.to);
    assert(board.to_fen() == before);
}

void test_capture_history_key_extraction_for_non_capture_fails() {
    sirio::Board board;
    const std::string before = board.to_fen();
    const sirio::Move quiet = sirio::move_from_uci(board, "g1f3");

    const auto key = sirio::make_capture_history_key_for_tests(board, quiet);
    assert(!key.has_value());
    assert(board.to_fen() == before);
}

void test_noisy_history_key_extraction_for_promotion_succeeds_and_is_deterministic() {
    sirio::Board board{"4k3/6P1/8/8/8/8/8/4K3 w - - 0 1"};
    const std::string before = board.to_fen();
    const sirio::Move promotion = sirio::move_from_uci(board, "g7g8q");

    const auto first = sirio::make_noisy_history_key_for_tests(board, promotion);
    const auto second = sirio::make_noisy_history_key_for_tests(board, promotion);
    assert(first.has_value());
    assert(second.has_value());
    assert(first->mover == sirio::Color::White);
    assert(first->mover_piece == sirio::PieceType::Pawn);
    assert(first->to == promotion.to);
    assert(first->mover == second->mover);
    assert(first->mover_piece == second->mover_piece);
    assert(first->to == second->to);
    assert(board.to_fen() == before);
}

void test_capture_history_key_extraction_for_invalid_move_fails() {
    sirio::Board board;
    const std::string before = board.to_fen();
    sirio::Move invalid{0, 63, sirio::PieceType::Pawn};
    invalid.captured = sirio::PieceType::Queen;

    const auto key = sirio::make_capture_history_key_for_tests(board, invalid);
    assert(!key.has_value());
    assert(board.to_fen() == before);
}



void test_continuation_history_key_extraction_valid_quiet_sequence_succeeds() {
    const sirio::Board previous_board;
    const auto previous_move = sirio::move_from_uci(previous_board, "e2e4");
    const sirio::Board current_board = previous_board.apply_move(previous_move);
    const auto current_move = sirio::move_from_uci(current_board, "e7e5");

    const std::string previous_before = previous_board.to_fen();
    const std::string current_before = current_board.to_fen();
    const auto key = sirio::make_continuation_history_key_for_tests(previous_board, previous_move, current_board, current_move);
    assert(key.has_value());
    assert(key->previous_mover_color == sirio::Color::White);
    assert(key->current_mover_color == sirio::Color::Black);
    assert(key->previous_moving_piece == sirio::PieceType::Pawn);
    assert(key->previous_to_square == previous_move.to);
    assert(key->current_moving_piece == sirio::PieceType::Pawn);
    assert(key->current_to_square == current_move.to);
    assert(previous_board.to_fen() == previous_before);
    assert(current_board.to_fen() == current_before);
}

void test_continuation_history_key_extraction_valid_capture_then_quiet_succeeds() {
    const sirio::Board previous_board{"4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1"};
    const auto previous_move = sirio::move_from_uci(previous_board, "e4d5");
    const sirio::Board current_board = previous_board.apply_move(previous_move);
    const auto current_move = sirio::move_from_uci(current_board, "e8e7");

    const auto key = sirio::make_continuation_history_key_for_tests(previous_board, previous_move, current_board, current_move);
    assert(key.has_value());
}

void test_continuation_history_key_extraction_missing_previous_context_fails() {
    const sirio::Board previous_board;
    const sirio::Board current_board;
    const auto current_move = sirio::move_from_uci(current_board, "e2e4");
    const auto key = sirio::make_continuation_history_key_for_tests(previous_board, std::nullopt, current_board, current_move);
    assert(!key.has_value());
}

void test_continuation_history_key_extraction_invalid_current_move_fails() {
    const sirio::Board previous_board;
    const auto previous_move = sirio::move_from_uci(previous_board, "e2e4");
    const sirio::Board current_board = previous_board.apply_move(previous_move);
    sirio::Move invalid_current{0, 56, sirio::PieceType::Knight};
    const auto key = sirio::make_continuation_history_key_for_tests(previous_board, previous_move, current_board, invalid_current);
    assert(!key.has_value());
}

void test_continuation_history_key_extraction_is_deterministic_and_context_sensitive() {
    const sirio::Board previous_board;
    const auto prev_a = sirio::move_from_uci(previous_board, "e2e4");
    const auto prev_b = sirio::move_from_uci(previous_board, "d2d4");
    const sirio::Board current_board_a = previous_board.apply_move(prev_a);
    const auto current_move_a = sirio::move_from_uci(current_board_a, "e7e5");
    const auto first = sirio::make_continuation_history_key_for_tests(previous_board, prev_a, current_board_a, current_move_a);
    const auto second = sirio::make_continuation_history_key_for_tests(previous_board, prev_a, current_board_a, current_move_a);
    assert(first.has_value() && second.has_value());
    assert(first->previous_to_square == second->previous_to_square);
    assert(first->current_to_square == second->current_to_square);

    const sirio::Board current_board_b = previous_board.apply_move(prev_b);
    const auto current_move_b = sirio::move_from_uci(current_board_b, "e7e5");
    const auto different = sirio::make_continuation_history_key_for_tests(previous_board, prev_b, current_board_b, current_move_b);
    assert(different.has_value());
    assert(first->previous_to_square != different->previous_to_square);
}

void test_search_history_aggregate_lifecycle_contract() {
    sirio::SearchHistory history;
    sirio::Board start;
    sirio::Board capture_board{"8/8/8/3p4/4P3/8/8/8 w - - 0 1"};
    sirio::Board promotion_board{"4k3/6P1/8/8/8/8/8/4K3 w - - 0 1"};

    const sirio::Move quiet = sirio::move_from_uci(start, "e2e4");
    const sirio::Move killer = sirio::move_from_uci(start, "d2d4");
    const sirio::Move capture = sirio::move_from_uci(capture_board, "e4d5");
    const sirio::Move noisy = sirio::move_from_uci(promotion_board, "g7g8q");
    const sirio::Move prev = sirio::move_from_uci(start, "g1f3");
    constexpr std::size_t correction_bucket = 77;

    assert(history.quiet_history_score(quiet, sirio::Color::White) == 0);
    assert(!history.killer_slots(2)[0].has_value());
    assert(!history.killer_slots(2)[1].has_value());
    assert(history.capture_history().score(capture, sirio::Color::White) == 0);
    assert(history.noisy_history().score(noisy, sirio::Color::White) == 0);
    assert(history.continuation_history().score(sirio::Color::White, prev, sirio::Color::White, quiet) == 0);
    assert(history.correction_history().score(sirio::Color::White, correction_bucket) == 0);

    history.update_quiet_history(sirio::Color::White, quiet, 3, true);
    history.store_killer(killer, 2);
    history.capture_history().update(sirio::Color::White, capture, 3, true);
    history.noisy_history().update(sirio::Color::White, noisy, 3, true);
    history.continuation_history().update(sirio::Color::White, prev, sirio::Color::White, quiet, 3, true);
    history.correction_history().update(sirio::Color::White, correction_bucket, 3, true);

    assert(history.quiet_history_score(quiet, sirio::Color::White) > 0);
    assert(history.killer_slots(2)[0].has_value());
    assert(history.capture_history().score(capture, sirio::Color::White) > 0);
    assert(history.noisy_history().score(noisy, sirio::Color::White) > 0);
    assert(history.continuation_history().score(sirio::Color::White, prev, sirio::Color::White, quiet) > 0);
    assert(history.correction_history().score(sirio::Color::White, correction_bucket) > 0);

    history.clear();

    assert(history.quiet_history_score(quiet, sirio::Color::White) == 0);
    assert(!history.killer_slots(2)[0].has_value());
    assert(!history.killer_slots(2)[1].has_value());
    assert(history.capture_history().score(capture, sirio::Color::White) == 0);
    assert(history.noisy_history().score(noisy, sirio::Color::White) == 0);
    assert(history.continuation_history().score(sirio::Color::White, prev, sirio::Color::White, quiet) == 0);
    assert(history.correction_history().score(sirio::Color::White, correction_bucket) == 0);
}

void test_search_history_aggregate_key_isolation_and_deterministic_cycles() {
    sirio::SearchHistory history;
    sirio::Board start;
    sirio::Board capture_board{"8/8/8/3p4/4P3/8/8/8 w - - 0 1"};
    sirio::Board promotion_board{"4k3/6P1/8/8/8/8/8/4K3 w - - 0 1"};

    const sirio::Move quiet_a = sirio::move_from_uci(start, "e2e4");
    const sirio::Move quiet_b = sirio::move_from_uci(start, "d2d4");
    const sirio::Move capture_a = sirio::move_from_uci(capture_board, "e4d5");
    const sirio::Move noisy_a = sirio::move_from_uci(promotion_board, "g7g8q");
    const sirio::Move prev = sirio::move_from_uci(start, "g1f3");
    constexpr std::size_t bucket_a = 9;
    constexpr std::size_t bucket_b = 10;

    history.update_quiet_history(sirio::Color::White, quiet_a, 2, true);
    history.store_killer(quiet_a, 4);
    history.capture_history().update(sirio::Color::White, capture_a, 2, true);
    history.noisy_history().update(sirio::Color::White, noisy_a, 2, true);
    history.continuation_history().update(sirio::Color::White, prev, sirio::Color::White, quiet_a, 2, true);
    history.correction_history().update(sirio::Color::White, bucket_a, 2, true);

    assert(history.quiet_history_score(quiet_b, sirio::Color::White) == 0);
    assert(!history.killer_slots(3)[0].has_value());
    assert(history.capture_history().score(capture_a, sirio::Color::Black) == 0);
    assert(history.noisy_history().score(noisy_a, sirio::Color::Black) == 0);
    assert(history.continuation_history().score(sirio::Color::White, quiet_a, sirio::Color::White, prev) == 0);
    assert(history.correction_history().score(sirio::Color::White, bucket_b) == 0);

    for (int i = 0; i < 3; ++i) {
        history.clear();
        history.update_quiet_history(sirio::Color::White, quiet_a, 2, true);
        history.store_killer(quiet_a, 4);
        history.capture_history().update(sirio::Color::White, capture_a, 2, true);
        history.noisy_history().update(sirio::Color::White, noisy_a, 2, true);
        history.continuation_history().update(sirio::Color::White, prev, sirio::Color::White, quiet_a, 2, true);
        history.correction_history().update(sirio::Color::White, bucket_a, 2, true);

        assert(history.quiet_history_score(quiet_a, sirio::Color::White) == 4);
        assert(history.killer_slots(4)[0].has_value());
        assert(history.capture_history().score(capture_a, sirio::Color::White) == 4);
        assert(history.noisy_history().score(noisy_a, sirio::Color::White) == 4);
        assert(history.continuation_history().score(sirio::Color::White, prev, sirio::Color::White, quiet_a) == 4);
        assert(history.correction_history().score(sirio::Color::White, bucket_a) == 4);
    }
}

void test_search_history_aggregate_key_contract_audit_no_search_use() {
    sirio::SearchHistory history;

    sirio::Board capture_board{"8/8/8/3p4/4P3/8/8/8 w - - 0 1"};
    const std::string capture_before = capture_board.to_fen();
    const sirio::Move capture_move = sirio::move_from_uci(capture_board, "e4d5");
    const auto capture_key_a = sirio::make_capture_history_key_for_tests(capture_board, capture_move);
    const auto capture_key_b = sirio::make_capture_history_key_for_tests(capture_board, capture_move);
    assert(capture_key_a.has_value() && capture_key_b.has_value());
    assert(capture_key_a->mover == capture_key_b->mover);
    assert(capture_key_a->attacker == capture_key_b->attacker);
    assert(capture_key_a->captured == capture_key_b->captured);
    assert(capture_key_a->to == capture_key_b->to);
    assert(capture_board.to_fen() == capture_before);

    sirio::Board promo_board{"4k3/6P1/8/8/8/8/8/4K3 w - - 0 1"};
    const std::string noisy_before = promo_board.to_fen();
    const sirio::Move noisy_move = sirio::move_from_uci(promo_board, "g7g8q");
    const auto noisy_key_a = sirio::make_noisy_history_key_for_tests(promo_board, noisy_move);
    const auto noisy_key_b = sirio::make_noisy_history_key_for_tests(promo_board, noisy_move);
    assert(noisy_key_a.has_value() && noisy_key_b.has_value());
    assert(noisy_key_a->mover == noisy_key_b->mover);
    assert(noisy_key_a->mover_piece == noisy_key_b->mover_piece);
    assert(noisy_key_a->to == noisy_key_b->to);
    assert(promo_board.to_fen() == noisy_before);

    const sirio::Board previous_board;
    const auto previous_move = sirio::move_from_uci(previous_board, "e2e4");
    const sirio::Board current_board = previous_board.apply_move(previous_move);
    const auto current_move = sirio::move_from_uci(current_board, "e7e5");
    const std::string previous_before = previous_board.to_fen();
    const std::string current_before = current_board.to_fen();
    const auto continuation_key_a = sirio::make_continuation_history_key_for_tests(
        previous_board, previous_move, current_board, current_move);
    const auto continuation_key_b = sirio::make_continuation_history_key_for_tests(
        previous_board, previous_move, current_board, current_move);
    assert(continuation_key_a.has_value() && continuation_key_b.has_value());
    assert(continuation_key_a->previous_mover_color == continuation_key_b->previous_mover_color);
    assert(continuation_key_a->current_mover_color == continuation_key_b->current_mover_color);
    assert(continuation_key_a->previous_moving_piece == continuation_key_b->previous_moving_piece);
    assert(continuation_key_a->previous_to_square == continuation_key_b->previous_to_square);
    assert(continuation_key_a->current_moving_piece == continuation_key_b->current_moving_piece);
    assert(continuation_key_a->current_to_square == continuation_key_b->current_to_square);
    assert(previous_board.to_fen() == previous_before);
    assert(current_board.to_fen() == current_before);

    const auto correction_key_a = sirio::make_correction_history_key_for_tests(sirio::Color::White, 2049);
    const auto correction_key_b = sirio::make_correction_history_key_for_tests(sirio::Color::White, 2049);
    assert(correction_key_a.has_value() && correction_key_b.has_value());
    assert(correction_key_a->mover_color == correction_key_b->mover_color);
    assert(correction_key_a->bucket == correction_key_b->bucket);

    sirio::Board quiet_board;
    const sirio::Move quiet_move = sirio::move_from_uci(quiet_board, "g1f3");
    assert(!sirio::make_capture_history_key_for_tests(quiet_board, quiet_move).has_value());
    assert(!sirio::make_noisy_history_key_for_tests(quiet_board, quiet_move).has_value());
    assert(!sirio::make_continuation_history_key_for_tests(previous_board, std::nullopt, current_board, current_move).has_value());
    const auto invalid_color = static_cast<sirio::Color>(99);
    assert(!sirio::make_correction_history_key_for_tests(invalid_color, 12).has_value());

    history.capture_history().update(capture_key_a->mover, capture_move, 2, true);
    history.noisy_history().update(noisy_key_a->mover, noisy_move, 2, true);
    history.continuation_history().update(continuation_key_a->previous_mover_color,
                                          previous_move,
                                          continuation_key_a->current_mover_color,
                                          current_move,
                                          2,
                                          true);
    history.correction_history().update(correction_key_a->mover_color, correction_key_a->bucket, 2, true);

    assert(history.capture_history().score(capture_move, capture_key_a->mover) == 4);
    assert(history.noisy_history().score(noisy_move, noisy_key_a->mover) == 4);
    assert(history.continuation_history().score(continuation_key_a->previous_mover_color,
                                                previous_move,
                                                continuation_key_a->current_mover_color,
                                                current_move) == 4);
    assert(history.correction_history().score(correction_key_a->mover_color, correction_key_a->bucket) == 4);

    history.clear();

    assert(history.capture_history().score(capture_move, capture_key_a->mover) == 0);
    assert(history.noisy_history().score(noisy_move, noisy_key_a->mover) == 0);
    assert(history.continuation_history().score(continuation_key_a->previous_mover_color,
                                                previous_move,
                                                continuation_key_a->current_mover_color,
                                                current_move) == 0);
    assert(history.correction_history().score(correction_key_a->mover_color, correction_key_a->bucket) == 0);
}

void test_search_history_clear_resets_continuation_history() {
    sirio::SearchHistory history;
    sirio::Board start;
    const sirio::Move e2e4 = sirio::move_from_uci(start, "e2e4");
    const sirio::Move g1f3 = sirio::move_from_uci(start, "g1f3");

    history.continuation_history().update(sirio::Color::White, e2e4, sirio::Color::White, g1f3, 3, true);
    assert(history.continuation_history().score(sirio::Color::White, e2e4, sirio::Color::White, g1f3) > 0);
    history.clear();
    assert(history.continuation_history().score(sirio::Color::White, e2e4, sirio::Color::White, g1f3) == 0);
}


std::string load_search_source_for_tests() {
    const std::vector<std::string> candidates{"src/search.cpp", "../src/search.cpp", "../../src/search.cpp",
                                              "../../../src/search.cpp"};
    for (const auto &path : candidates) {
        std::ifstream in(path);
        if (!in.good()) {
            continue;
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
        const std::string source = buffer.str();
        if (!source.empty()) {
            return source;
        }
    }
    return {};
}

void test_search_main_negamax_uses_read_only_correction_history_hook() {
    const std::string source = load_search_source_for_tests();
    assert(!source.empty());

    assert(source.find("make_correction_history_key_from_position(board)") != std::string::npos);
    assert(source.find("apply_correction_history_to_static_eval(raw_static_eval, context.history, correction_key)") !=
           std::string::npos);
    assert(source.find("apply_correction_history_quiet_beta_cutoff_update(context.history") != std::string::npos);
    assert(source.find("apply_correction_history_fail_low_update(context.history") != std::string::npos);
    assert(source.find("apply_correction_history_quiet_beta_cutoff_update_for_tests") == std::string::npos);
    assert(source.find("apply_correction_history_fail_low_update_for_tests") == std::string::npos);
    assert(source.find("new_entry.static_eval = raw_static_eval;") != std::string::npos);
}

void test_search_qsearch_has_no_correction_history_wiring() {
    const std::string source = load_search_source_for_tests();
    assert(!source.empty());
    const std::size_t qsearch_pos = source.find("int quiescence(");
    assert(qsearch_pos != std::string::npos);
    const std::string qsearch_source = source.substr(qsearch_pos);

    assert(qsearch_source.find("make_correction_history_key_from_position") == std::string::npos);
    assert(qsearch_source.find("apply_correction_history_to_static_eval") == std::string::npos);
    assert(qsearch_source.find("correction_history()") == std::string::npos);
}

void test_search_selectivity_foundation_flags_contract() {
    assert(sirio::search_params::selectivity_reverse_futility_enabled);
    assert(!sirio::search_params::selectivity_move_count_pruning_enabled);
    assert(!sirio::search_params::selectivity_probcut_enabled);
    assert(!sirio::search_params::selectivity_singular_extensions_enabled);
}

void test_search_selectivity_foundation_helpers_contract() {
    assert(sirio::search_params::selectivity_reverse_futility_is_enabled());
    assert(!sirio::search_params::selectivity_move_count_pruning_is_enabled());
    assert(!sirio::search_params::selectivity_probcut_is_enabled());
    assert(!sirio::search_params::selectivity_singular_extensions_are_enabled());
}

void test_reverse_futility_helper_allows_pruning_only_when_guards_and_margin_pass() {
    constexpr int depth = 1;
    const int margin = sirio::search_params::reverse_futility_margin(depth, false);
    const int beta = 300;
    assert(sirio::search_params::should_apply_reverse_futility_pruning(
        depth, beta + margin, beta, false, false, false, false));
    assert(!sirio::search_params::should_apply_reverse_futility_pruning(
        depth, beta + margin - 1, beta, false, false, false, false));
}

void test_reverse_futility_margin_helper_is_deterministic_and_non_negative() {
    constexpr int depth = 5;
    const int first = sirio::search_params::reverse_futility_margin(depth, false);
    const int second = sirio::search_params::reverse_futility_margin(depth, false);
    const int improving_first = sirio::search_params::reverse_futility_margin(depth, true);
    const int improving_second = sirio::search_params::reverse_futility_margin(depth, true);

    assert(first == second);
    assert(improving_first == improving_second);
    assert(first >= 0);
    assert(improving_first >= 0);
}

void test_reverse_futility_margin_helper_depth_progression_is_stable() {
    const int depth1 = sirio::search_params::reverse_futility_margin(1, false);
    const int depth2 = sirio::search_params::reverse_futility_margin(2, false);
    const int depth3 = sirio::search_params::reverse_futility_margin(3, false);

    assert(depth2 >= depth1);
    assert(depth3 >= depth2);
}

void test_reverse_futility_margin_helper_improving_mode_is_deterministic() {
    constexpr int depth = 4;
    const int not_improving = sirio::search_params::reverse_futility_margin(depth, false);
    const int improving = sirio::search_params::reverse_futility_margin(depth, true);

    assert(improving >= 0);
    assert(not_improving >= 0);
    assert(improving <= not_improving);
}

void test_reverse_futility_helper_in_check_is_disabled() {
    assert(!sirio::search_params::should_apply_reverse_futility_pruning(3, 800, 300, true, true, false, false));
}

void test_reverse_futility_helper_pv_node_is_disabled() {
    assert(!sirio::search_params::should_apply_reverse_futility_pruning(3, 800, 300, true, false, true, false));
}

void test_reverse_futility_helper_root_node_is_disabled() {
    assert(!sirio::search_params::should_apply_reverse_futility_pruning(3, 800, 300, true, false, false, true));
}

void test_reverse_futility_helper_invalid_depth_is_disabled() {
    assert(!sirio::search_params::should_apply_reverse_futility_pruning(0, 800, 300, true, false, false, false));
    assert(!sirio::search_params::should_apply_reverse_futility_pruning(-2, 800, 300, false, false, false, false));
    assert(!sirio::search_params::should_apply_reverse_futility_pruning(
        sirio::search_params::reverse_futility_depth_limit + 1,
        800,
        300,
        false,
        false,
        false,
        false));
}

void test_reverse_futility_helper_no_side_effects_or_history_dependency() {
    sirio::SearchHistory history;
    const auto key = sirio::make_correction_history_key_for_tests(sirio::Color::White, 99);
    assert(key.has_value());
    history.correction_history().update(*key, 27);
    const int before = history.correction_history().score(*key);
    assert(!sirio::search_params::should_apply_reverse_futility_pruning(3, 800, 300, true, false, false, false));
    assert(history.correction_history().score(*key) == before);
}

void test_move_count_pruning_helper_is_disabled_by_default_flag() {
    assert(!sirio::search_params::should_apply_move_count_pruning(4, 12, false, false, false, false));
}

void test_move_count_pruning_helper_in_check_is_disabled() {
    assert(!sirio::search_params::should_apply_move_count_pruning(4, 12, false, true, false, false));
}

void test_move_count_pruning_helper_pv_node_is_disabled() {
    assert(!sirio::search_params::should_apply_move_count_pruning(4, 12, false, false, true, false));
}

void test_move_count_pruning_helper_root_node_is_disabled() {
    assert(!sirio::search_params::should_apply_move_count_pruning(4, 12, false, false, false, true));
}

void test_move_count_pruning_helper_invalid_depth_is_disabled() {
    assert(!sirio::search_params::should_apply_move_count_pruning(0, 12, false, false, false, false));
    assert(!sirio::search_params::should_apply_move_count_pruning(-1, 12, true, false, false, false));
}

void test_move_count_pruning_helper_invalid_move_count_is_disabled() {
    assert(!sirio::search_params::should_apply_move_count_pruning(3, 0, false, false, false, false));
    assert(!sirio::search_params::should_apply_move_count_pruning(3, -2, true, false, false, false));
}
void test_reverse_futility_return_observability_counter_lifecycle() {
    sirio::SearchHistory history;
    assert(history.reverse_futility_return_count_for_tests() == 0);
    history.record_reverse_futility_return();
    assert(history.reverse_futility_return_count_for_tests() == 1);
    history.clear();
    assert(history.reverse_futility_return_count_for_tests() == 0);
}

void test_search_main_negamax_has_guarded_reverse_futility_return_scaffold_wiring() {
    const std::string source = load_search_source_for_tests();
    assert(!source.empty());
    const std::size_t negamax_pos = source.find("int negamax(");
    assert(negamax_pos != std::string::npos);
    const std::size_t qsearch_pos = source.find("int quiescence(");
    assert(qsearch_pos != std::string::npos);
    const std::string negamax_source = source.substr(negamax_pos, qsearch_pos - negamax_pos);

    assert(negamax_source.find("should_apply_reverse_futility_pruning(") != std::string::npos);
    assert(negamax_source.find("if (search_params::should_apply_reverse_futility_pruning(") != std::string::npos);
    assert(negamax_source.find("in_check,\n            pv_node,\n            ply == 0") != std::string::npos);
    assert(negamax_source.find("context.history.record_reverse_futility_return();") != std::string::npos);
    assert(negamax_source.find("return corrected_static_eval;") != std::string::npos);
}

void test_search_qsearch_has_no_reverse_futility_pruning_wiring() {
    const std::string source = load_search_source_for_tests();
    assert(!source.empty());
    const std::size_t qsearch_pos = source.find("int quiescence(");
    assert(qsearch_pos != std::string::npos);
    const std::string qsearch_source = source.substr(qsearch_pos);

    assert(qsearch_source.find("should_apply_reverse_futility_pruning(") == std::string::npos);
    assert(qsearch_source.find("record_reverse_futility_return") == std::string::npos);
    assert(qsearch_source.find("return corrected_static_eval;") == std::string::npos);
}

void test_search_reverse_futility_return_is_guarded_and_localized() {
    const std::string source = load_search_source_for_tests();
    assert(!source.empty());
    const std::size_t negamax_pos = source.find("int negamax(");
    assert(negamax_pos != std::string::npos);
    const std::size_t qsearch_pos = source.find("int quiescence(");
    assert(qsearch_pos != std::string::npos);
    const std::string negamax_source = source.substr(negamax_pos, qsearch_pos - negamax_pos);

    const std::size_t guard_pos = negamax_source.find("if (search_params::should_apply_reverse_futility_pruning(");
    assert(guard_pos != std::string::npos);
    const std::size_t return_pos = negamax_source.find("return corrected_static_eval;", guard_pos);
    assert(return_pos != std::string::npos);

    const std::size_t unguarded_return_pos = negamax_source.find("return corrected_static_eval;");
    assert(unguarded_return_pos == return_pos);
    assert(negamax_source.find("_for_tests(") == std::string::npos);
}

void test_search_main_negamax_has_disabled_move_count_pruning_probe_wiring() {
    const std::string source = load_search_source_for_tests();
    assert(!source.empty());

    const std::size_t negamax_pos = source.find("int negamax(");
    assert(negamax_pos != std::string::npos);
    const std::size_t qsearch_pos = source.find("int quiescence(");
    assert(qsearch_pos != std::string::npos);
    const std::string negamax_source = source.substr(negamax_pos, qsearch_pos - negamax_pos);

    assert(negamax_source.find("should_apply_move_count_pruning(") != std::string::npos);
    assert(negamax_source.find("const bool move_count_pruning_probe =") != std::string::npos);
    assert(negamax_source.find("(void)move_count_pruning_probe;") != std::string::npos);
    assert(negamax_source.find("searched_move_count") != std::string::npos);
    assert(negamax_source.find("if (move_count_pruning_probe)") == std::string::npos);
    const std::size_t probe_pos = negamax_source.find("const bool move_count_pruning_probe =");
    assert(probe_pos != std::string::npos);
    const std::string probe_window = negamax_source.substr(probe_pos, 400);
    assert(probe_window.find("continue;") == std::string::npos);
    assert(probe_window.find("break;") == std::string::npos);
    assert(probe_window.find("return") == std::string::npos);
}

void test_search_qsearch_has_no_move_count_pruning_runtime_wiring() {
    const std::string source = load_search_source_for_tests();
    assert(!source.empty());
    const std::size_t qsearch_pos = source.find("int quiescence(");
    assert(qsearch_pos != std::string::npos);
    const std::string qsearch_source = source.substr(qsearch_pos);

    assert(qsearch_source.find("should_apply_move_count_pruning(") == std::string::npos);
}

}  // namespace


void test_capture_noisy_update_policy_capture_success_and_failure() {
    sirio::SearchHistory history;
    sirio::Board board{"8/8/8/3p4/4P3/8/8/8 w - - 0 1"};
    const sirio::Move capture = sirio::move_from_uci(board, "e4d5");
    const auto capture_key = sirio::make_capture_history_key_for_tests(board, capture);

    const auto success = sirio::make_capture_noisy_history_update(capture_key, std::nullopt, true, 3);
    assert(success.target == sirio::CaptureNoisyHistoryUpdateTarget::Capture);
    assert(success.capture_key.has_value());
    assert(success.bonus == 9);
    sirio::apply_capture_noisy_history_update_for_tests(history, success);
    const int after_success = history.capture_history().score(capture, sirio::Color::White);
    assert(after_success > 0);

    const auto failure = sirio::make_capture_noisy_history_update(capture_key, std::nullopt, false, 3);
    assert(failure.target == sirio::CaptureNoisyHistoryUpdateTarget::Capture);
    sirio::apply_capture_noisy_history_update_for_tests(history, failure);
    assert(history.capture_history().score(capture, sirio::Color::White) < after_success);
}

void test_capture_noisy_update_policy_noisy_success_and_quiet_rejection() {
    sirio::SearchHistory history;
    sirio::Board noisy_board{"4k3/6P1/8/8/8/8/8/4K3 w - - 0 1"};
    sirio::Board quiet_board;
    const sirio::Move noisy = sirio::move_from_uci(noisy_board, "g7g8q");
    const sirio::Move quiet = sirio::move_from_uci(quiet_board, "e2e4");

    const auto noisy_key = sirio::make_noisy_history_key_for_tests(noisy_board, noisy);
    const auto noisy_update = sirio::make_capture_noisy_history_update(std::nullopt, noisy_key, true, 2);
    assert(noisy_update.target == sirio::CaptureNoisyHistoryUpdateTarget::Noisy);
    sirio::apply_capture_noisy_history_update_for_tests(history, noisy_update);
    assert(history.noisy_history().score(noisy, sirio::Color::White) > 0);

    const auto quiet_noisy_key = sirio::make_noisy_history_key_for_tests(quiet_board, quiet);
    const auto quiet_update = sirio::make_capture_noisy_history_update(std::nullopt, quiet_noisy_key, true, 2);
    assert(quiet_update.target == sirio::CaptureNoisyHistoryUpdateTarget::None);
}

void test_capture_noisy_update_policy_invalid_key_clamp_and_determinism() {
    sirio::SearchHistory history;
    const sirio::CaptureHistoryKey invalid_capture{sirio::Color::White, sirio::PieceType::Pawn, sirio::PieceType::Pawn, 99};
    const auto invalid = sirio::make_capture_noisy_history_update(invalid_capture, std::nullopt, true, 3);
    assert(invalid.target == sirio::CaptureNoisyHistoryUpdateTarget::None);

    sirio::Board board{"8/8/8/3p4/4P3/8/8/8 w - - 0 1"};
    const sirio::Move capture = sirio::move_from_uci(board, "e4d5");
    const auto key = sirio::make_capture_history_key_for_tests(board, capture);
    const auto first = sirio::make_capture_noisy_history_update(key, std::nullopt, true, 100000);
    const auto second = sirio::make_capture_noisy_history_update(key, std::nullopt, true, 100000);
    assert(first.bonus == sirio::search_params::history_bonus_limit);
    assert(first.bonus == second.bonus);

    for (int i = 0; i < 2000; ++i) {
        sirio::apply_capture_noisy_history_update_for_tests(history, first);
    }
    assert(history.capture_history().score(capture, sirio::Color::White) == sirio::search_params::history_max);
}



void test_capture_noisy_runtime_apply_noop_for_invalid_update() {
    sirio::SearchHistory history;
    sirio::Board board{"8/8/8/3p4/4P3/8/8/8 w - - 0 1"};
    const sirio::Move capture = sirio::move_from_uci(board, "e4d5");
    const int before = history.capture_history().score(capture, sirio::Color::White);

    const auto update = sirio::make_capture_noisy_history_update(std::nullopt, std::nullopt, true, 4);
    assert(update.target == sirio::CaptureNoisyHistoryUpdateTarget::None);
    sirio::apply_capture_noisy_history_update(history, update);

    assert(history.capture_history().score(capture, sirio::Color::White) == before);
}

void test_capture_noisy_shadow_event_capture_success_failure_and_reset() {
    sirio::SearchHistory history;
    sirio::Board board{"8/8/8/3p4/4P3/8/8/8 w - - 0 1"};
    const sirio::Move capture = sirio::move_from_uci(board, "e4d5");
    const auto capture_key = sirio::make_capture_history_key_for_tests(board, capture);
    assert(capture_key.has_value());

    const auto success_event = sirio::make_capture_noisy_history_update_event_for_tests(
        sirio::CaptureNoisyHistoryUpdateTarget::Capture, capture_key, std::nullopt, 3, true,
        "simulated beta cutoff capture");
    sirio::apply_capture_noisy_history_update_event_for_tests(history, success_event);
    const int after_success = history.capture_history().score(capture, sirio::Color::White);
    assert(after_success > 0);

    const auto failure_event = sirio::make_capture_noisy_history_update_event_for_tests(
        sirio::CaptureNoisyHistoryUpdateTarget::Capture, capture_key, std::nullopt, 3, false,
        "simulated fail-low capture");
    sirio::apply_capture_noisy_history_update_event_for_tests(history, failure_event);
    assert(history.capture_history().score(capture, sirio::Color::White) < after_success);

    history.clear();
    assert(history.capture_history().score(capture, sirio::Color::White) == 0);
}

void test_capture_noisy_shadow_event_noisy_success_quiet_and_invalid_reject() {
    sirio::SearchHistory history;
    sirio::Board noisy_board{"4k3/6P1/8/8/8/8/8/4K3 w - - 0 1"};
    sirio::Board quiet_board;
    const sirio::Move noisy = sirio::move_from_uci(noisy_board, "g7g8q");
    const sirio::Move quiet = sirio::move_from_uci(quiet_board, "e2e4");

    const auto noisy_key = sirio::make_noisy_history_key_for_tests(noisy_board, noisy);
    const auto noisy_event = sirio::make_capture_noisy_history_update_event_for_tests(
        sirio::CaptureNoisyHistoryUpdateTarget::Noisy, std::nullopt, noisy_key, 2, true,
        "simulated promotion success");
    sirio::apply_capture_noisy_history_update_event_for_tests(history, noisy_event);
    assert(history.noisy_history().score(noisy, sirio::Color::White) > 0);

    const auto quiet_noisy_key = sirio::make_noisy_history_key_for_tests(quiet_board, quiet);
    const auto quiet_event = sirio::make_capture_noisy_history_update_event_for_tests(
        sirio::CaptureNoisyHistoryUpdateTarget::Noisy, std::nullopt, quiet_noisy_key, 2, true,
        "quiet should be ignored");
    sirio::apply_capture_noisy_history_update_event_for_tests(history, quiet_event);
    assert(quiet_noisy_key == std::nullopt);

    const sirio::CaptureHistoryKey invalid_capture{sirio::Color::White, sirio::PieceType::Pawn, sirio::PieceType::Pawn, 99};
    const auto invalid_event = sirio::make_capture_noisy_history_update_event_for_tests(
        sirio::CaptureNoisyHistoryUpdateTarget::Capture, invalid_capture, std::nullopt, 3, true,
        "invalid square");
    const int before_invalid = history.noisy_history().score(noisy, sirio::Color::White);
    sirio::apply_capture_noisy_history_update_event_for_tests(history, invalid_event);
    assert(history.noisy_history().score(noisy, sirio::Color::White) == before_invalid);
}

void test_capture_noisy_shadow_event_sequence_deterministic_and_no_search_invocation() {
    sirio::SearchHistory first;
    sirio::SearchHistory second;
    sirio::Board board{"8/8/8/3p4/4P3/8/8/8 w - - 0 1"};
    const std::string before = board.to_fen();
    const sirio::Move capture = sirio::move_from_uci(board, "e4d5");
    const auto capture_key = sirio::make_capture_history_key_for_tests(board, capture);

    std::array<sirio::CaptureNoisyHistoryUpdateEvent, 3> events{
        sirio::make_capture_noisy_history_update_event_for_tests(
            sirio::CaptureNoisyHistoryUpdateTarget::Capture, capture_key, std::nullopt, 4, true,
            "seq-1"),
        sirio::make_capture_noisy_history_update_event_for_tests(
            sirio::CaptureNoisyHistoryUpdateTarget::Capture, capture_key, std::nullopt, 4, false,
            "seq-2"),
        sirio::make_capture_noisy_history_update_event_for_tests(
            sirio::CaptureNoisyHistoryUpdateTarget::Capture, capture_key, std::nullopt, 4, true,
            "seq-3")};

    for (const auto &event : events) {
        sirio::apply_capture_noisy_history_update_event_for_tests(first, event);
        sirio::apply_capture_noisy_history_update_event_for_tests(second, event);
    }

    assert(first.capture_history().score(capture, sirio::Color::White) ==
           second.capture_history().score(capture, sirio::Color::White));
    assert(board.to_fen() == before);
}

void test_capture_noisy_runtime_observability_applies_once_for_main_tactical_cutoff() {
    sirio::SearchHistory history;
    sirio::Board board{"8/8/8/3p4/4P3/8/8/8 w - - 0 1"};
    const sirio::Move capture = sirio::move_from_uci(board, "e4d5");
    const auto capture_key = sirio::make_capture_history_key_for_tests(board, capture);
    const auto noisy_key = sirio::make_noisy_history_key_for_tests(board, capture);
    assert(capture_key.has_value());
    assert(noisy_key.has_value());

    const bool applied = sirio::apply_capture_noisy_runtime_update_for_tests(
        history, sirio::CaptureNoisyRuntimeUpdateSite::MainNegamaxTacticalBetaCutoff, capture_key, noisy_key, 3);
    assert(applied);
    assert(history.capture_noisy_runtime_update_counters().applied == 1);
    assert(history.capture_history().score(capture, sirio::Color::White) > 0);
}

void test_capture_noisy_runtime_observability_excluded_paths_do_not_apply() {
    sirio::SearchHistory history;
    sirio::Board tactical_board{"8/8/8/3p4/4P3/8/8/8 w - - 0 1"};
    const sirio::Move tactical = sirio::move_from_uci(tactical_board, "e4d5");
    const auto capture_key = sirio::make_capture_history_key_for_tests(tactical_board, tactical);
    const auto noisy_key = sirio::make_noisy_history_key_for_tests(tactical_board, tactical);
    assert(capture_key.has_value());
    assert(noisy_key.has_value());

    sirio::Board quiet_board;
    const sirio::Move quiet = sirio::move_from_uci(quiet_board, "e2e4");
    const auto quiet_capture_key = sirio::make_capture_history_key_for_tests(quiet_board, quiet);
    const auto quiet_noisy_key = sirio::make_noisy_history_key_for_tests(quiet_board, quiet);
    assert(!quiet_capture_key.has_value());
    assert(!quiet_noisy_key.has_value());

    assert(!sirio::apply_capture_noisy_runtime_update_for_tests(
        history, sirio::CaptureNoisyRuntimeUpdateSite::MainNegamaxQuietBetaCutoff, capture_key, noisy_key, 3));
    assert(!sirio::apply_capture_noisy_runtime_update_for_tests(
        history, sirio::CaptureNoisyRuntimeUpdateSite::QuiescenceTacticalBetaCutoff, capture_key, noisy_key, 3));
    assert(!sirio::apply_capture_noisy_runtime_update_for_tests(
        history, sirio::CaptureNoisyRuntimeUpdateSite::FailedTacticalMove, capture_key, noisy_key, 3));
    assert(!sirio::apply_capture_noisy_runtime_update_for_tests(
        history, sirio::CaptureNoisyRuntimeUpdateSite::MainNegamaxTacticalBetaCutoff, quiet_capture_key,
        quiet_noisy_key, 3));

    assert(history.capture_noisy_runtime_update_counters().applied == 0);
    assert(history.capture_history().score(tactical, sirio::Color::White) == 0);
}

void test_capture_noisy_runtime_observability_deterministic_repeated_sequence() {
    sirio::SearchHistory first;
    sirio::SearchHistory second;
    sirio::Board board{"8/8/8/3p4/4P3/8/8/8 w - - 0 1"};
    const sirio::Move capture = sirio::move_from_uci(board, "e4d5");
    const auto capture_key = sirio::make_capture_history_key_for_tests(board, capture);
    const auto noisy_key = sirio::make_noisy_history_key_for_tests(board, capture);
    assert(capture_key.has_value());
    assert(noisy_key.has_value());

    for (int i = 0; i < 3; ++i) {
        assert(sirio::apply_capture_noisy_runtime_update_for_tests(
            first, sirio::CaptureNoisyRuntimeUpdateSite::MainNegamaxTacticalBetaCutoff, capture_key, noisy_key, 4));
        assert(sirio::apply_capture_noisy_runtime_update_for_tests(
            second, sirio::CaptureNoisyRuntimeUpdateSite::MainNegamaxTacticalBetaCutoff, capture_key, noisy_key, 4));
    }

    assert(first.capture_noisy_runtime_update_counters().applied == 3);
    assert(second.capture_noisy_runtime_update_counters().applied == 3);
    assert(first.capture_history().score(capture, sirio::Color::White) ==
           second.capture_history().score(capture, sirio::Color::White));
}

void test_continuation_runtime_observability_main_quiet_beta_cutoff_applies_once() {
    sirio::SearchHistory history;
    sirio::Board previous_board{"8/8/8/8/8/8/4K2r/4N1k1 b - - 0 1"};
    sirio::Board current_board{"8/8/8/8/8/8/4K3/4N2k w - - 0 1"};
    const sirio::Move previous_move = sirio::move_from_uci(previous_board, "h2h1");
    const sirio::Move current_move = sirio::move_from_uci(current_board, "e1d3");
    const auto key = sirio::make_continuation_history_key_for_tests(previous_board, previous_move, current_board, current_move);
    assert(key.has_value());

    assert(sirio::apply_continuation_runtime_update_for_tests(
        history, sirio::ContinuationRuntimeUpdateSite::MainNegamaxQuietBetaCutoff, key, {}, 3));
    assert(history.continuation_quiet_beta_cutoff_update_count_for_tests() == 1);
    assert(history.continuation_quiet_beta_cutoff_skip_count_for_tests() == 0);
    assert(history.continuation_quiet_beta_cutoff_malus_count_for_tests() == 0);
}

void test_continuation_runtime_observability_excluded_or_invalid_paths_skip_or_noop() {
    sirio::SearchHistory history;
    assert(!sirio::apply_continuation_runtime_update_for_tests(
        history, sirio::ContinuationRuntimeUpdateSite::QuiescenceQuietBetaCutoff, std::nullopt, {}, 3));
    assert(!sirio::apply_continuation_runtime_update_for_tests(
        history, sirio::ContinuationRuntimeUpdateSite::MainNegamaxCaptureBetaCutoff, std::nullopt, {}, 3));
    assert(!sirio::apply_continuation_runtime_update_for_tests(
        history, sirio::ContinuationRuntimeUpdateSite::MainNegamaxPromotionBetaCutoff, std::nullopt, {}, 3));
    assert(!sirio::apply_continuation_runtime_update_for_tests(
        history, sirio::ContinuationRuntimeUpdateSite::MainNegamaxQuietNonCutoff, std::nullopt, {}, 3));
    assert(!sirio::apply_continuation_runtime_update_for_tests(
        history, sirio::ContinuationRuntimeUpdateSite::MainNegamaxQuietBetaCutoff, std::nullopt, {}, 3));

    assert(history.continuation_quiet_beta_cutoff_update_count_for_tests() == 0);
    assert(history.continuation_quiet_beta_cutoff_skip_count_for_tests() == 1);
}

void test_continuation_runtime_observability_clear_resets_and_is_deterministic() {
    sirio::SearchHistory a;
    sirio::SearchHistory b;
    sirio::Board previous_board{"8/8/8/8/8/8/4K2r/4N1k1 b - - 0 1"};
    sirio::Board current_board{"8/8/8/8/8/8/4K3/4N2k w - - 0 1"};
    const sirio::Move previous_move = sirio::move_from_uci(previous_board, "h2h1");
    const sirio::Move current_move = sirio::move_from_uci(current_board, "e1d3");
    const auto key = sirio::make_continuation_history_key_for_tests(previous_board, previous_move, current_board, current_move);
    assert(key.has_value());

    assert(sirio::apply_continuation_runtime_update_for_tests(
        a, sirio::ContinuationRuntimeUpdateSite::MainNegamaxQuietBetaCutoff, key, {}, 3));
    assert(sirio::apply_continuation_runtime_update_for_tests(
        b, sirio::ContinuationRuntimeUpdateSite::MainNegamaxQuietBetaCutoff, key, {}, 3));
    a.clear();
    b.clear();
    assert(sirio::apply_continuation_runtime_update_for_tests(
        a, sirio::ContinuationRuntimeUpdateSite::MainNegamaxQuietBetaCutoff, key, {}, 3));
    assert(sirio::apply_continuation_runtime_update_for_tests(
        b, sirio::ContinuationRuntimeUpdateSite::MainNegamaxQuietBetaCutoff, key, {}, 3));
    assert(a.continuation_quiet_beta_cutoff_update_count_for_tests() == 1);
    assert(b.continuation_quiet_beta_cutoff_update_count_for_tests() == 1);
}

void test_continuation_runtime_observability_applies_malus_to_tried_quiets_only() {
    sirio::SearchHistory history;
    sirio::Board previous_board{"8/8/8/8/8/8/4K2r/4N1k1 b - - 0 1"};
    sirio::Board current_board{"8/8/8/8/8/8/4K3/4N2k w - - 0 1"};
    const sirio::Move previous_move = sirio::move_from_uci(previous_board, "h2h1");
    const sirio::Move cutoff_move = sirio::move_from_uci(current_board, "e1d3");
    const sirio::Move tried_a = sirio::move_from_uci(current_board, "e1c2");
    const sirio::Move tried_b = sirio::move_from_uci(current_board, "e2d3");
    const auto cutoff_key =
        sirio::make_continuation_history_key_for_tests(previous_board, previous_move, current_board, cutoff_move);
    const auto tried_key_a =
        sirio::make_continuation_history_key_for_tests(previous_board, previous_move, current_board, tried_a);
    const auto tried_key_b =
        sirio::make_continuation_history_key_for_tests(previous_board, previous_move, current_board, tried_b);
    assert(cutoff_key.has_value());
    assert(tried_key_a.has_value());
    assert(tried_key_b.has_value());
    std::array<sirio::ContinuationHistoryKey, 2> tried_keys{*tried_key_a, *tried_key_b};

    assert(sirio::apply_continuation_runtime_update_for_tests(
        history, sirio::ContinuationRuntimeUpdateSite::MainNegamaxQuietBetaCutoff, cutoff_key, tried_keys, 3));

    assert(history.continuation_quiet_beta_cutoff_update_count_for_tests() == 1);
    assert(history.continuation_quiet_beta_cutoff_malus_count_for_tests() == 2);
    assert(history.continuation_history().score(cutoff_key->previous_mover_color, previous_move,
                                                cutoff_key->current_mover_color, cutoff_move) > 0);
    assert(history.continuation_history().score(tried_key_a->previous_mover_color, previous_move,
                                                tried_key_a->current_mover_color, tried_a) < 0);
    assert(history.continuation_history().score(tried_key_b->previous_mover_color, previous_move,
                                                tried_key_b->current_mover_color, tried_b) < 0);
}
void test_correction_runtime_observability_quiet_beta_cutoff_applies_once_positive_raw_baseline() {
    sirio::SearchHistory history;
    const auto key = sirio::make_correction_history_key_for_tests(sirio::Color::White, 9);
    assert(key.has_value());
    assert(sirio::apply_correction_history_quiet_beta_cutoff_update_for_tests(history, key, 20, 40));
    assert(history.correction_quiet_beta_cutoff_update_count_for_tests() == 1);
    assert(history.correction_history().score(*key) == 5);
}

void test_correction_runtime_observability_quiet_beta_cutoff_clamps_large_positive_delta() {
    sirio::SearchHistory history;
    const auto key = sirio::make_correction_history_key_for_tests(sirio::Color::White, 109);
    assert(key.has_value());
    assert(sirio::apply_correction_history_quiet_beta_cutoff_update_for_tests(history, key, -500, 500));
    assert(history.correction_quiet_beta_cutoff_update_count_for_tests() == 1);
    assert(history.correction_history().score(*key) == sirio::search_params::correction_history_runtime_delta_max);
}

void test_correction_runtime_observability_excluded_paths_do_not_apply() {
    sirio::SearchHistory history;
    const auto key = sirio::make_correction_history_key_for_tests(sirio::Color::White, 10);
    assert(key.has_value());
    assert(!sirio::apply_correction_history_quiet_beta_cutoff_update_for_tests(history, std::nullopt, 10, 30));
    assert(!sirio::apply_correction_history_quiet_beta_cutoff_update_for_tests(history, key, 20, 20));
    assert(!sirio::apply_correction_history_quiet_beta_cutoff_update_for_tests(history, key, 21, 20));
    assert(!sirio::apply_correction_history_quiet_beta_cutoff_update_for_tests(history, key, 10, 13));
    assert(history.correction_quiet_beta_cutoff_update_count_for_tests() == 0);
    assert(history.correction_history().score(*key) == 0);
}

void test_correction_runtime_observability_clear_resets_and_is_deterministic() {
    sirio::SearchHistory a;
    sirio::SearchHistory b;
    const auto key = sirio::make_correction_history_key_for_tests(sirio::Color::Black, 44);
    assert(key.has_value());
    assert(sirio::apply_correction_history_quiet_beta_cutoff_update_for_tests(a, key, -10, 10));
    assert(sirio::apply_correction_history_quiet_beta_cutoff_update_for_tests(b, key, -10, 10));
    a.clear();
    b.clear();
    assert(a.correction_quiet_beta_cutoff_update_count_for_tests() == 0);
    assert(b.correction_quiet_beta_cutoff_update_count_for_tests() == 0);
    assert(a.correction_history().score(*key) == 0);
    assert(b.correction_history().score(*key) == 0);
    assert(sirio::apply_correction_history_quiet_beta_cutoff_update_for_tests(a, key, -10, 10));
    assert(sirio::apply_correction_history_quiet_beta_cutoff_update_for_tests(b, key, -10, 10));
    assert(a.correction_history().score(*key) == b.correction_history().score(*key));
    assert(a.correction_quiet_beta_cutoff_update_count_for_tests() == 1);
}
void test_correction_runtime_observability_fail_low_applies_once_negative_raw_baseline() {
    sirio::SearchHistory history;
    const auto key = sirio::make_correction_history_key_for_tests(sirio::Color::White, 19);
    assert(key.has_value());
    assert(sirio::apply_correction_history_fail_low_update_for_tests(history, key, 40, 20));
    assert(history.correction_fail_low_update_count_for_tests() == 1);
    assert(history.correction_history().score(*key) == -5);
}

void test_correction_runtime_observability_fail_low_clamps_large_negative_delta() {
    sirio::SearchHistory history;
    const auto key = sirio::make_correction_history_key_for_tests(sirio::Color::Black, 119);
    assert(key.has_value());
    assert(sirio::apply_correction_history_fail_low_update_for_tests(history, key, 500, -500));
    assert(history.correction_fail_low_update_count_for_tests() == 1);
    assert(history.correction_history().score(*key) == -sirio::search_params::correction_history_runtime_delta_max);
}

void test_correction_runtime_observability_fail_low_excluded_paths_do_not_apply() {
    sirio::SearchHistory history;
    const auto key = sirio::make_correction_history_key_for_tests(sirio::Color::White, 20);
    assert(key.has_value());
    assert(!sirio::apply_correction_history_fail_low_update_for_tests(history, std::nullopt, 40, 12));
    assert(!sirio::apply_correction_history_fail_low_update_for_tests(history, key, 40, 40));
    assert(!sirio::apply_correction_history_fail_low_update_for_tests(history, key, 40, 55));
    assert(!sirio::apply_correction_history_fail_low_update_for_tests(history, key, 40, 38));
    assert(history.correction_fail_low_update_count_for_tests() == 0);
    assert(history.correction_history().score(*key) == 0);
}

void test_correction_runtime_observability_fail_low_clear_resets_and_is_deterministic() {
    sirio::SearchHistory a;
    sirio::SearchHistory b;
    const auto key = sirio::make_correction_history_key_for_tests(sirio::Color::Black, 45);
    assert(key.has_value());
    assert(sirio::apply_correction_history_fail_low_update_for_tests(a, key, 30, 10));
    assert(sirio::apply_correction_history_fail_low_update_for_tests(b, key, 30, 10));
    a.clear();
    b.clear();
    assert(a.correction_fail_low_update_count_for_tests() == 0);
    assert(b.correction_fail_low_update_count_for_tests() == 0);
    assert(a.correction_history().score(*key) == 0);
    assert(b.correction_history().score(*key) == 0);
    assert(sirio::apply_correction_history_fail_low_update_for_tests(a, key, 30, 10));
    assert(sirio::apply_correction_history_fail_low_update_for_tests(b, key, 30, 10));
    assert(a.correction_history().score(*key) == b.correction_history().score(*key));
    assert(a.correction_fail_low_update_count_for_tests() == 1);
}

void run_history_tests() {
    test_initial_state_neutral_and_empty_killers();
    test_is_quiet_move_predicate();
    test_quiet_history_update_bonus_clamp_and_saturation();
    test_store_killer_slots_and_duplicate_handling();
    test_isolation_between_entries();
    test_capture_history_scaffold_basics();
    test_capture_history_clamp_and_indexing();
    test_noisy_history_scaffold_basics_and_determinism();
    test_history_clear_resets_quiet_killer_capture_and_noisy();
    test_continuation_history_default_update_and_clear();
    test_continuation_history_clamp_indexing_and_determinism();
    test_search_history_aggregate_lifecycle_contract();
    test_search_history_aggregate_key_isolation_and_deterministic_cycles();
    test_search_history_aggregate_key_contract_audit_no_search_use();
    test_search_history_clear_resets_continuation_history();
    test_search_main_negamax_uses_read_only_correction_history_hook();
    test_search_qsearch_has_no_correction_history_wiring();
    test_search_selectivity_foundation_flags_contract();
    test_search_selectivity_foundation_helpers_contract();
    test_reverse_futility_helper_allows_pruning_only_when_guards_and_margin_pass();
    test_reverse_futility_margin_helper_is_deterministic_and_non_negative();
    test_reverse_futility_margin_helper_depth_progression_is_stable();
    test_reverse_futility_margin_helper_improving_mode_is_deterministic();
    test_reverse_futility_helper_in_check_is_disabled();
    test_reverse_futility_helper_pv_node_is_disabled();
    test_reverse_futility_helper_root_node_is_disabled();
    test_reverse_futility_helper_invalid_depth_is_disabled();
    test_reverse_futility_helper_no_side_effects_or_history_dependency();
    test_move_count_pruning_helper_is_disabled_by_default_flag();
    test_move_count_pruning_helper_in_check_is_disabled();
    test_move_count_pruning_helper_pv_node_is_disabled();
    test_move_count_pruning_helper_root_node_is_disabled();
    test_move_count_pruning_helper_invalid_depth_is_disabled();
    test_move_count_pruning_helper_invalid_move_count_is_disabled();
    test_reverse_futility_return_observability_counter_lifecycle();
    test_search_main_negamax_has_guarded_reverse_futility_return_scaffold_wiring();
    test_search_qsearch_has_no_reverse_futility_pruning_wiring();
    test_search_reverse_futility_return_is_guarded_and_localized();
    test_search_main_negamax_has_disabled_move_count_pruning_probe_wiring();
    test_search_qsearch_has_no_move_count_pruning_runtime_wiring();
    test_correction_history_default_update_clamp_and_clear();
    test_correction_history_bucket_indexing_and_determinism();
    test_search_history_clear_resets_correction_history();
    test_correction_history_key_extraction_valid_white_and_black();
    test_correction_history_key_extraction_bucket_normalization_and_aliases();
    test_correction_history_key_extraction_invalid_color_fails();
    test_correction_history_key_extraction_is_deterministic();
    test_correction_history_key_targets_expected_bucket_for_update_and_read();
    test_correction_history_static_eval_helper_zero_state_returns_raw_eval();
    test_correction_history_static_eval_helper_invalid_key_returns_raw_eval();
    test_correction_history_static_eval_helper_seeded_delta_and_clear_contract();
    test_correction_history_static_eval_helper_is_read_only();
    test_correction_history_position_key_same_structure_and_side_is_deterministic();
    test_correction_history_position_key_changes_with_side_to_move();
    test_correction_history_position_key_changes_with_pawn_structure();
    test_correction_history_position_key_helper_is_read_only_and_clear_independent();
    test_noisy_history_key_extraction_for_quiet_move_fails();
    test_capture_history_key_extraction_for_capture_succeeds();
    test_capture_history_key_extraction_for_non_capture_fails();
    test_noisy_history_key_extraction_for_promotion_succeeds_and_is_deterministic();
    test_capture_history_key_extraction_for_invalid_move_fails();
    test_continuation_history_key_extraction_valid_quiet_sequence_succeeds();
    test_continuation_history_key_extraction_valid_capture_then_quiet_succeeds();
    test_continuation_history_key_extraction_missing_previous_context_fails();
    test_continuation_history_key_extraction_invalid_current_move_fails();
    test_continuation_history_key_extraction_is_deterministic_and_context_sensitive();
    test_capture_noisy_update_policy_capture_success_and_failure();
    test_capture_noisy_update_policy_noisy_success_and_quiet_rejection();
    test_capture_noisy_update_policy_invalid_key_clamp_and_determinism();
    test_capture_noisy_runtime_apply_noop_for_invalid_update();
    test_capture_noisy_shadow_event_capture_success_failure_and_reset();
    test_capture_noisy_shadow_event_noisy_success_quiet_and_invalid_reject();
    test_capture_noisy_shadow_event_sequence_deterministic_and_no_search_invocation();
    test_capture_noisy_runtime_observability_applies_once_for_main_tactical_cutoff();
    test_capture_noisy_runtime_observability_excluded_paths_do_not_apply();
    test_capture_noisy_runtime_observability_deterministic_repeated_sequence();
    test_continuation_runtime_observability_main_quiet_beta_cutoff_applies_once();
    test_continuation_runtime_observability_excluded_or_invalid_paths_skip_or_noop();
    test_continuation_runtime_observability_clear_resets_and_is_deterministic();
    test_continuation_runtime_observability_applies_malus_to_tried_quiets_only();
    test_correction_runtime_observability_quiet_beta_cutoff_applies_once_positive_raw_baseline();
    test_correction_runtime_observability_quiet_beta_cutoff_clamps_large_positive_delta();
    test_correction_runtime_observability_excluded_paths_do_not_apply();
    test_correction_runtime_observability_clear_resets_and_is_deterministic();
    test_correction_runtime_observability_fail_low_applies_once_negative_raw_baseline();
    test_correction_runtime_observability_fail_low_clamps_large_negative_delta();
    test_correction_runtime_observability_fail_low_excluded_paths_do_not_apply();
    test_correction_runtime_observability_fail_low_clear_resets_and_is_deterministic();
}
