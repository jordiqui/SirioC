#include "engine/eval/nnue/accumulator.hpp"

#include "engine/core/board.hpp"
#include "engine/eval/nnue/evaluator.hpp"

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
        if (c == '/' || std::isdigit(static_cast<unsigned char>(c))) {
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

int32_t clamp_to_i32(int64_t value) {
    return static_cast<int32_t>(std::clamp<int64_t>(value,
                                                    std::numeric_limits<int32_t>::min(),
                                                    std::numeric_limits<int32_t>::max()));
}

} // namespace

void Accumulator::reset() noexcept {
    hidden_.clear();
    hidden_.shrink_to_fit();
    features_.clear();
    features_.shrink_to_fit();
    cached_fen_.clear();
    cached_input_dim_ = 0;
    cached_hidden_dim_ = 0;
    initialized_ = false;
}

void Accumulator::update(const engine::Board& board, const FeatureTransformer& transformer) {
    const uint32_t input_dim = transformer.input_dim;
    const uint32_t hidden_dim = transformer.output_dim;
    const std::string& fen = board.last_fen();

    const bool dims_changed = cached_input_dim_ != input_dim || cached_hidden_dim_ != hidden_dim;
    if (!dims_changed && initialized_ && fen == cached_fen_) {
        return;
    }

    cached_input_dim_ = input_dim;
    cached_hidden_dim_ = hidden_dim;
    cached_fen_ = fen;

    if (hidden_dim == 0 || input_dim == 0 || !transformer.valid()) {
        hidden_.assign(hidden_dim, 0);
        features_.assign(input_dim, 0);
        initialized_ = hidden_dim == 0 || input_dim == 0;
        return;
    }

    std::vector<int32_t> new_features = build_features(board, input_dim);

    if (features_.size() != input_dim) {
        features_.assign(input_dim, 0);
        initialized_ = false;
    }
    if (hidden_.size() != hidden_dim) {
        hidden_.assign(hidden_dim, 0);
        initialized_ = false;
    }

    if (!initialized_) {
        for (uint32_t h = 0; h < hidden_dim; ++h) {
            int64_t sum = transformer.bias[h];
            const std::size_t base = static_cast<std::size_t>(h) * static_cast<std::size_t>(input_dim);
            for (uint32_t i = 0; i < input_dim; ++i) {
                sum += static_cast<int64_t>(transformer.weights[base + i]) * static_cast<int64_t>(new_features[i]);
            }
            hidden_[h] = clamp_to_i32(sum);
        }
        initialized_ = true;
    } else {
        for (uint32_t h = 0; h < hidden_dim; ++h) {
            int64_t sum = hidden_[h];
            const std::size_t base = static_cast<std::size_t>(h) * static_cast<std::size_t>(input_dim);
            for (uint32_t i = 0; i < input_dim; ++i) {
                const int64_t delta = static_cast<int64_t>(new_features[i]) - static_cast<int64_t>(features_[i]);
                if (delta != 0) {
                    sum += static_cast<int64_t>(transformer.weights[base + i]) * delta;
                }
            }
            hidden_[h] = clamp_to_i32(sum);
        }
    }

    features_ = std::move(new_features);
}

} // namespace engine::nnue
