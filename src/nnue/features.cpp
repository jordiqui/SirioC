#include "sirio/nnue/features.hpp"

#include <array>

namespace sirio::nnue {

namespace {

constexpr std::array<std::uint32_t, 5> kOwnChannels = {
    static_cast<std::uint32_t>(SirioHalfKAv1RelativeChannel::OwnPawn),
    static_cast<std::uint32_t>(SirioHalfKAv1RelativeChannel::OwnKnight),
    static_cast<std::uint32_t>(SirioHalfKAv1RelativeChannel::OwnBishop),
    static_cast<std::uint32_t>(SirioHalfKAv1RelativeChannel::OwnRook),
    static_cast<std::uint32_t>(SirioHalfKAv1RelativeChannel::OwnQueen),
};

constexpr std::array<std::uint32_t, 5> kEnemyChannels = {
    static_cast<std::uint32_t>(SirioHalfKAv1RelativeChannel::EnemyPawn),
    static_cast<std::uint32_t>(SirioHalfKAv1RelativeChannel::EnemyKnight),
    static_cast<std::uint32_t>(SirioHalfKAv1RelativeChannel::EnemyBishop),
    static_cast<std::uint32_t>(SirioHalfKAv1RelativeChannel::EnemyRook),
    static_cast<std::uint32_t>(SirioHalfKAv1RelativeChannel::EnemyQueen),
};

constexpr std::array<PieceType, 5> kNonKingPieceTypes = {
    PieceType::Pawn, PieceType::Knight, PieceType::Bishop, PieceType::Rook, PieceType::Queen,
};

}  // namespace

int perspective_square(Color perspective, int square) {
    if (square < 0 || square >= static_cast<int>(kSirioHalfKAv1SquareCount)) {
        return -1;
    }
    if (perspective == Color::White) {
        return square;
    }
    const int file = file_of(square);
    const int rank = rank_of(square);
    const int flipped_rank = 7 - rank;
    return flipped_rank * 8 + file;
}

bool relative_piece_channel(Color perspective, Color piece_color, PieceType piece_type,
                            std::uint32_t &out_channel) {
    if (piece_type == PieceType::King || piece_type == PieceType::Count) {
        return false;
    }

    const std::uint32_t piece_index = static_cast<std::uint32_t>(piece_type);
    if (piece_index >= kOwnChannels.size()) {
        return false;
    }

    const bool own_piece = piece_color == perspective;
    out_channel = own_piece ? kOwnChannels[piece_index] : kEnemyChannels[piece_index];
    return true;
}

std::uint32_t make_feature_index(int perspective_king_square, std::uint32_t relative_piece_channel,
                                 int perspective_piece_square) {
    return (static_cast<std::uint32_t>(perspective_king_square) *
                kSirioHalfKAv1RelativeChannelCount +
            relative_piece_channel) *
               kSirioHalfKAv1SquareCount +
           static_cast<std::uint32_t>(perspective_piece_square);
}

bool encode_sirio_halfka_v1(const Board &board, SparseFeatureState &out_state) {
    out_state.clear();

    const int white_king_square = board.king_square(Color::White);
    const int black_king_square = board.king_square(Color::Black);
    if (white_king_square < 0 || black_king_square < 0) {
        return false;
    }

    for (std::uint32_t perspective_idx = 0; perspective_idx < kSirioHalfKAv1PerspectiveCount;
         ++perspective_idx) {
        const Color perspective = perspective_idx == 0 ? Color::White : Color::Black;
        const int king_square = perspective == Color::White ? white_king_square : black_king_square;
        const int perspective_king_square = perspective_square(perspective, king_square);
        if (perspective_king_square < 0) {
            return false;
        }

        auto &perspective_state = out_state.perspectives[perspective_idx];
        for (Color piece_color : {Color::White, Color::Black}) {
            for (PieceType piece_type : kNonKingPieceTypes) {
                std::uint32_t channel = 0;
                if (!relative_piece_channel(perspective, piece_color, piece_type, channel)) {
                    continue;
                }
                for (int piece_square : board.piece_list(piece_color, piece_type)) {
                    const int perspective_piece_square = perspective_square(perspective, piece_square);
                    if (perspective_piece_square < 0) {
                        return false;
                    }
                    const std::uint32_t feature_index =
                        make_feature_index(perspective_king_square, channel, perspective_piece_square);
                    if (!perspective_state.push(SparseFeature{feature_index, 1})) {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

}  // namespace sirio::nnue
