#include "sirio/nnue/backend.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "sirio/move.hpp"

namespace sirio::nnue {

namespace {
constexpr int max_feature_value = 64;

int color_index(Color color) { return color == Color::White ? 0 : 1; }

int feature_offset(Color color, PieceType type) {
    return color_index(color) * static_cast<int>(kPieceTypeCount) +
           static_cast<int>(type);
}

void clamp_non_negative(int &value) {
    if (value < 0) {
        value = 0;
    }
}

}  // namespace

SingleNetworkBackend::SingleNetworkBackend() = default;

bool SingleNetworkBackend::load(const std::string &path, std::string *error_message) {
    std::ifstream input(path);
    if (!input) {
        if (error_message) {
            *error_message = "Unable to open NNUE file: " + path;
        }
        loaded_ = false;
        path_.clear();
        return false;
    }

    std::string header;
    if (!(input >> header) || header != "SirioNNUE1") {
        if (error_message) {
            *error_message = "Unrecognized NNUE header";
        }
        loaded_ = false;
        path_.clear();
        return false;
    }

    if (!(input >> params_.bias >> params_.scale)) {
        if (error_message) {
            *error_message = "Failed to read NNUE bias and scale";
        }
        loaded_ = false;
        path_.clear();
        return false;
    }

    for (double &weight : params_.piece_weights) {
        if (!(input >> weight)) {
            if (error_message) {
                *error_message = "Incomplete NNUE weight table";
            }
            loaded_ = false;
            path_.clear();
            return false;
        }
    }

    loaded_ = true;
    path_ = path;
    return true;
}

FeatureState SingleNetworkBackend::compute_state(const Board &board) const {
    FeatureState state{};
    for (int color_idx = 0; color_idx < 2; ++color_idx) {
        const Color color = color_idx == 0 ? Color::White : Color::Black;
        for (std::size_t type_idx = 0; type_idx < kPieceTypeCount; ++type_idx) {
            const PieceType type = static_cast<PieceType>(type_idx);
            const Bitboard pieces = board.pieces(color, type);
            const int count = static_cast<int>(std::popcount(pieces));
            state.piece_counts[color_idx * kPieceTypeCount + type_idx] =
                std::clamp(count, 0, max_feature_value);
        }
    }
    return state;
}

void SingleNetworkBackend::apply_move_to_state(FeatureState &state, const Board &previous,
                                               const Move &move, const Board &current) {
    (void)current;
    const Color mover = previous.side_to_move();
    const int mover_offset = feature_offset(mover, move.piece);
    // Captures remove material from the opponent.
    if (move.captured.has_value()) {
        const Color victim_color = opposite(mover);
        const PieceType victim_type = *move.captured;
        const int victim_offset = feature_offset(victim_color, victim_type);
        clamp_non_negative(state.piece_counts[victim_offset]);
        if (state.piece_counts[victim_offset] > 0) {
            --state.piece_counts[victim_offset];
        }
    }

    // Promotions transform a pawn into another piece.
    if (move.piece == PieceType::Pawn && move.promotion.has_value()) {
        clamp_non_negative(state.piece_counts[mover_offset]);
        if (state.piece_counts[mover_offset] > 0) {
            --state.piece_counts[mover_offset];
        }
        const int promoted_offset = feature_offset(mover, *move.promotion);
        if (state.piece_counts[promoted_offset] < max_feature_value) {
            ++state.piece_counts[promoted_offset];
        }
    }
}

void SingleNetworkBackend::initialize(const Board &board) {
    stack_.clear();
    stack_.push_back(compute_state(board));
}

void SingleNetworkBackend::reset(const Board &board) { initialize(board); }

void SingleNetworkBackend::push(const Board &previous, const std::optional<Move> &move,
                                const Board &current) {
    if (stack_.empty()) {
        stack_.push_back(compute_state(previous));
    }
    FeatureState next = stack_.back();
    if (move.has_value()) {
        apply_move_to_state(next, previous, *move, current);
    }
    stack_.push_back(next);
}

void SingleNetworkBackend::pop() {
    if (stack_.size() > 1) {
        stack_.pop_back();
    }
}

int SingleNetworkBackend::evaluate(const Board &board) {
    if (stack_.empty()) {
        stack_.push_back(compute_state(board));
    }
    if (!loaded_) {
        return 0;
    }
    const FeatureState &state = stack_.back();
    double value = params_.bias;
    for (std::size_t index = 0; index < state.piece_counts.size(); ++index) {
        value += params_.piece_weights[index] * static_cast<double>(state.piece_counts[index]);
    }
    value *= params_.scale;
    return static_cast<int>(std::lround(value));
}

}  // namespace sirio::nnue

