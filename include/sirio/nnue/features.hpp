#pragma once

#include <cstdint>

#include "sirio/board.hpp"
#include "sirio/nnue/backend.hpp"

namespace sirio::nnue {

inline constexpr const char *kSirioHalfKAv1Id = "SirioHalfKAv1";
inline constexpr std::uint32_t kSirioHalfKAv1PerspectiveCount = 2;
inline constexpr std::uint32_t kSirioHalfKAv1RelativeChannelCount = 10;
inline constexpr std::uint32_t kSirioHalfKAv1SquareCount = 64;
inline constexpr std::uint32_t kSirioHalfKAv1FeaturesPerPerspective =
    kSirioHalfKAv1SquareCount * kSirioHalfKAv1RelativeChannelCount * kSirioHalfKAv1SquareCount;

enum class SirioHalfKAv1RelativeChannel : std::uint32_t {
    OwnPawn = 0,
    OwnKnight = 1,
    OwnBishop = 2,
    OwnRook = 3,
    OwnQueen = 4,
    EnemyPawn = 5,
    EnemyKnight = 6,
    EnemyBishop = 7,
    EnemyRook = 8,
    EnemyQueen = 9,
};

[[nodiscard]] int perspective_square(Color perspective, int square);
[[nodiscard]] bool relative_piece_channel(Color perspective, Color piece_color, PieceType piece_type,
                                          std::uint32_t &out_channel);
[[nodiscard]] std::uint32_t make_feature_index(int perspective_king_square,
                                               std::uint32_t relative_piece_channel,
                                               int perspective_piece_square);
[[nodiscard]] bool encode_sirio_halfka_v1(const Board &board, SparseFeatureState &out_state);

enum class SirioHalfKAv1FullRefreshReason : std::uint8_t {
    None = 0,
    WhiteKingMoved = 1,
    BlackKingMoved = 2,
    BothKingsMoved = 3,
    InvalidInput = 4,
};

struct SirioHalfKAv1FeatureDiff {
    std::vector<SparseFeature> white_removed;
    std::vector<SparseFeature> white_added;
    std::vector<SparseFeature> black_removed;
    std::vector<SparseFeature> black_added;
    bool full_refresh_required = false;
    SirioHalfKAv1FullRefreshReason full_refresh_reason =
        SirioHalfKAv1FullRefreshReason::None;
};

[[nodiscard]] bool compute_sirio_halfka_v1_feature_diff(
    const Board &before, const Board &after, SirioHalfKAv1FeatureDiff &out_diff);


}  // namespace sirio::nnue
