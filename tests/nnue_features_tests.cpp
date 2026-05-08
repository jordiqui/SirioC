#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <unordered_set>

#include "sirio/board.hpp"
#include "sirio/nnue/features.hpp"

namespace {

void test_constants_contract() {
    static_assert(sirio::nnue::kSirioHalfKAv1FeaturesPerPerspective == 40960);
    static_assert(sirio::nnue::kSirioHalfKAv1RelativeChannelCount == 10);
}

void test_start_position_sparse_features() {
    sirio::Board board;
    sirio::nnue::SparseFeatureState state;
    assert(sirio::nnue::encode_sirio_halfka_v1(board, state));

    for (const auto &perspective : state.perspectives) {
        assert(perspective.count == 30);
        for (std::size_t i = 0; i < perspective.count; ++i) {
            assert(perspective.active[i].index < sirio::nnue::kSirioHalfKAv1FeaturesPerPerspective);
            assert(perspective.active[i].value == 1);
        }
    }
}

void test_kings_only_position() {
    sirio::Board board{"8/8/8/8/8/8/6k1/6K1 w - - 0 1"};
    sirio::nnue::SparseFeatureState state;
    assert(sirio::nnue::encode_sirio_halfka_v1(board, state));
    assert(state.perspectives[0].count == 0);
    assert(state.perspectives[1].count == 0);
}

void test_controlled_index_and_transform_determinism() {
    sirio::Board board{"4k3/8/8/8/8/8/P7/4K3 w - - 0 1"};
    sirio::nnue::SparseFeatureState state;
    assert(sirio::nnue::encode_sirio_halfka_v1(board, state));

    const int white_king = sirio::nnue::perspective_square(sirio::Color::White, 4);
    const int pawn_white = sirio::nnue::perspective_square(sirio::Color::White, 8);
    const std::uint32_t expected_white = sirio::nnue::make_feature_index(
        white_king, static_cast<std::uint32_t>(sirio::nnue::SirioHalfKAv1RelativeChannel::OwnPawn), pawn_white);

    assert(state.perspectives[0].count == 1);
    assert(state.perspectives[0].active[0].index == expected_white);

    const int black_king = sirio::nnue::perspective_square(sirio::Color::Black, 60);
    const int pawn_black = sirio::nnue::perspective_square(sirio::Color::Black, 8);
    const std::uint32_t expected_black = sirio::nnue::make_feature_index(
        black_king, static_cast<std::uint32_t>(sirio::nnue::SirioHalfKAv1RelativeChannel::EnemyPawn), pawn_black);

    assert(state.perspectives[1].count == 1);
    assert(state.perspectives[1].active[0].index == expected_black);

    assert(sirio::nnue::perspective_square(sirio::Color::White, 8) == 8);
    assert(sirio::nnue::perspective_square(sirio::Color::Black, 8) == 48);
}

void test_no_duplicate_indices() {
    sirio::Board board;
    sirio::nnue::SparseFeatureState state;
    assert(sirio::nnue::encode_sirio_halfka_v1(board, state));

    for (const auto &perspective : state.perspectives) {
        std::unordered_set<std::uint32_t> seen;
        for (std::size_t i = 0; i < perspective.count; ++i) {
            const bool inserted = seen.insert(perspective.active[i].index).second;
            assert(inserted);
        }
    }
}

void test_encoding_clears_previous_state() {
    sirio::nnue::SparseFeatureState state;
    assert(state.perspectives[0].push(sirio::nnue::SparseFeature{1234, 1}));
    assert(state.perspectives[1].push(sirio::nnue::SparseFeature{2345, 1}));

    sirio::Board kings_only{"8/8/8/8/8/8/6k1/6K1 w - - 0 1"};
    assert(sirio::nnue::encode_sirio_halfka_v1(kings_only, state));
    assert(state.perspectives[0].count == 0);
    assert(state.perspectives[1].count == 0);
}

}  // namespace

void run_nnue_features_tests() {
    test_constants_contract();
    test_start_position_sparse_features();
    test_kings_only_position();
    test_controlled_index_and_transform_determinism();
    test_no_duplicate_indices();
    test_encoding_clears_previous_state();
}
