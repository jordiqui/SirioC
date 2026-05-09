#include <cassert>

#include "sirio/board.hpp"
#include "sirio/move.hpp"
#include "sirio/nnue/features.hpp"

namespace {

void assert_feature_bounds(const std::vector<sirio::nnue::SparseFeature> &features) {
    for (const auto &feature : features) {
        assert(feature.index < sirio::nnue::kSirioHalfKAv1FeaturesPerPerspective);
        assert(feature.value == 1);
    }
}

void test_no_change_position() {
    sirio::Board board;
    sirio::nnue::SirioHalfKAv1FeatureDiff diff;
    assert(sirio::nnue::compute_sirio_halfka_v1_feature_diff(board, board, diff));
    assert(diff.white_removed.empty());
    assert(diff.white_added.empty());
    assert(diff.black_removed.empty());
    assert(diff.black_added.empty());
    assert(!diff.full_refresh_required);
}

void test_quiet_non_king_move_deterministic() {
    sirio::Board before;
    const sirio::Move move = sirio::move_from_uci(before, "e2e4");
    const sirio::Board after = before.apply_move(move);

    sirio::nnue::SirioHalfKAv1FeatureDiff diff1;
    sirio::nnue::SirioHalfKAv1FeatureDiff diff2;
    assert(sirio::nnue::compute_sirio_halfka_v1_feature_diff(before, after, diff1));
    assert(sirio::nnue::compute_sirio_halfka_v1_feature_diff(before, after, diff2));

    assert(!diff1.white_removed.empty() || !diff1.black_removed.empty());
    assert(!diff1.white_added.empty() || !diff1.black_added.empty());
    assert(!diff1.full_refresh_required);
    assert(diff1.white_removed == diff2.white_removed);
    assert(diff1.white_added == diff2.white_added);
    assert(diff1.black_removed == diff2.black_removed);
    assert(diff1.black_added == diff2.black_added);
}

void test_capture_position() {
    const sirio::Board before{"4k3/8/8/3pP3/8/8/8/4K3 w - - 0 1"};
    const sirio::Board after{"4k3/8/3P4/8/8/8/8/4K3 b - - 0 1"};
    sirio::nnue::SirioHalfKAv1FeatureDiff diff;
    assert(sirio::nnue::compute_sirio_halfka_v1_feature_diff(before, after, diff));
    assert(!diff.full_refresh_required);
    assert(!diff.white_removed.empty());
    assert(!diff.black_removed.empty());
}

void test_promotion_like_position() {
    const sirio::Board before{"4k3/6P1/8/8/8/8/8/4K3 w - - 0 1"};
    const sirio::Board after{"4k1Q1/8/8/8/8/8/8/4K3 b - - 0 1"};
    sirio::nnue::SirioHalfKAv1FeatureDiff diff;
    assert(sirio::nnue::compute_sirio_halfka_v1_feature_diff(before, after, diff));
    assert(!diff.full_refresh_required);
    assert(!diff.white_removed.empty());
    assert(!diff.white_added.empty());
}

void test_castling_like_position_requires_refresh() {
    const sirio::Board before{"4k3/8/8/8/8/8/8/4K2R w - - 0 1"};
    const sirio::Board after{"4k3/8/8/8/8/8/8/5RK1 b - - 0 1"};
    sirio::nnue::SirioHalfKAv1FeatureDiff diff;
    assert(sirio::nnue::compute_sirio_halfka_v1_feature_diff(before, after, diff));
    assert(diff.full_refresh_required);
    assert(diff.full_refresh_reason == sirio::nnue::SirioHalfKAv1FullRefreshReason::WhiteKingMoved);
}

void test_king_move_requires_refresh() {
    const sirio::Board before{"4k3/8/8/8/8/8/8/4K3 w - - 0 1"};
    const sirio::Board after{"4k3/8/8/8/8/8/4K3/8 b - - 0 1"};
    sirio::nnue::SirioHalfKAv1FeatureDiff diff;
    assert(sirio::nnue::compute_sirio_halfka_v1_feature_diff(before, after, diff));
    assert(diff.full_refresh_required);
}

void test_side_to_move_only_difference() {
    const sirio::Board white{"4k3/8/8/8/8/8/4P3/4K3 w - - 0 1"};
    const sirio::Board black{"4k3/8/8/8/8/8/4P3/4K3 b - - 0 1"};
    sirio::nnue::SirioHalfKAv1FeatureDiff diff;
    assert(sirio::nnue::compute_sirio_halfka_v1_feature_diff(white, black, diff));
    assert(diff.white_removed.empty());
    assert(diff.white_added.empty());
    assert(diff.black_removed.empty());
    assert(diff.black_added.empty());
    assert(!diff.full_refresh_required);
}

void test_feature_ranges_for_all_lists() {
    const sirio::Board before{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
    const sirio::Board after{"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1"};
    sirio::nnue::SirioHalfKAv1FeatureDiff diff;
    assert(sirio::nnue::compute_sirio_halfka_v1_feature_diff(before, after, diff));
    assert_feature_bounds(diff.white_removed);
    assert_feature_bounds(diff.white_added);
    assert_feature_bounds(diff.black_removed);
    assert_feature_bounds(diff.black_added);
}

}  // namespace

void run_nnue_feature_diff_v2_tests() {
    test_no_change_position();
    test_quiet_non_king_move_deterministic();
    test_capture_position();
    test_promotion_like_position();
    test_castling_like_position_requires_refresh();
    test_king_move_requires_refresh();
    test_side_to_move_only_difference();
    test_feature_ranges_for_all_lists();
}
