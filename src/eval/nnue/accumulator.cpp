#include "engine/eval/nnue/accumulator.hpp"

#include "engine/core/board.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace engine::nnue {

namespace {

std::vector<int32_t> build_features(const engine::Board& board, uint32_t input_dim) {
    std::vector<int32_t> features(input_dim, 0);
    if (input_dim == 0) {
        return features;
    }

    const std::string fen = board.last_fen();
    const auto space_pos = fen.find(' ');
    const std::string board_part = space_pos == std::string::npos ? fen : fen.substr(0, space_pos);
    std::string stm_field = "w";
    if (space_pos != std::string::npos) {
        const auto rest = fen.substr(space_pos + 1);
        const auto space_pos2 = rest.find(' ');
        stm_field = rest.substr(0, space_pos2);
    }

    std::array<int32_t, 12> counts{};
    for (char c : board_part) {
        if (c == '/') {
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            continue;
        }
        constexpr std::array<char, 12> piece_order{
            'P', 'N', 'B', 'R', 'Q', 'K',
            'p', 'n', 'b', 'r', 'q', 'k'};
        const auto it = std::find(piece_order.begin(), piece_order.end(), c);
        if (it != piece_order.end()) {
            const size_t piece_idx = static_cast<size_t>(std::distance(piece_order.begin(), it));
            counts[piece_idx] += 1;
        }
    }

    const uint32_t features_to_copy = std::min<uint32_t>(input_dim, static_cast<uint32_t>(counts.size()));
    for (uint32_t i = 0; i < features_to_copy; ++i) {
        features[i] = counts[i];
    }

    if (input_dim > counts.size()) {
        const int32_t stm_value = (stm_field == "w") ? 1 : -1;
        features[counts.size()] = stm_value;
    }
    if (input_dim > counts.size() + 1) {
        features[counts.size() + 1] = 1; // bias term
    }
    return features;
}

} // namespace

void Accumulator::reset() noexcept {
    hidden_.clear();
    hidden_.shrink_to_fit();
    cached_fen_.clear();
    cached_input_dim_ = 0;
    cached_hidden_dim_ = 0;
}

void Accumulator::update(const engine::Board& board,
                         uint32_t input_dim,
                         uint32_t hidden_dim,
                         const std::vector<int16_t>& feature_weights,
                         const std::vector<int32_t>& feature_bias) {
    const std::string& fen = board.last_fen();
    const bool needs_resize = hidden_.size() != hidden_dim;
    const bool dims_changed = cached_input_dim_ != input_dim || cached_hidden_dim_ != hidden_dim;
    if (!needs_resize && !dims_changed && fen == cached_fen_) {
        return;
    }

    hidden_.assign(hidden_dim, 0);
    cached_input_dim_ = input_dim;
    cached_hidden_dim_ = hidden_dim;
    cached_fen_ = fen;

    if (hidden_dim == 0 || input_dim == 0) {
        return;
    }

    const std::vector<int32_t> features = build_features(board, input_dim);
    const size_t expected_weights = static_cast<size_t>(input_dim) * static_cast<size_t>(hidden_dim);
    if (feature_weights.size() != expected_weights || feature_bias.size() != hidden_dim) {
        hidden_.assign(hidden_dim, 0);
        return;
    }

    for (uint32_t h = 0; h < hidden_dim; ++h) {
        int64_t sum = feature_bias[h];
        for (uint32_t i = 0; i < input_dim; ++i) {
            const size_t index = static_cast<size_t>(h) * static_cast<size_t>(input_dim) + static_cast<size_t>(i);
            sum += static_cast<int64_t>(feature_weights[index]) * static_cast<int64_t>(features[i]);
        }
        hidden_[h] = static_cast<int32_t>(std::clamp<int64_t>(sum,
                                                              std::numeric_limits<int32_t>::min(),
                                                              std::numeric_limits<int32_t>::max()));
    }
}

} // namespace engine::nnue
