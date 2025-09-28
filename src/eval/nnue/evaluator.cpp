#include "engine/eval/nnue/evaluator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>

#include "engine/core/board.hpp"
#include "engine/eval/nnue/accumulator.hpp"

namespace engine::nnue {

bool Evaluator::load_network(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        loaded_ = false;
        loaded_path_.clear();
        return false;
    }
    std::string header;
    if (!std::getline(file, header)) {
        loaded_ = false;
        loaded_path_.clear();
        return false;
    }
    if (!header.empty() && header.back() == '\r') header.pop_back();
    if (header != "SirioC SimpleNNUE v1") {
        loaded_ = false;
        loaded_path_.clear();
        return false;
    }

    std::string bias_label;
    double bias_value = 0.0;
    if (!(file >> bias_label >> bias_value) || bias_label != "bias") {
        loaded_ = false;
        loaded_path_.clear();
        return false;
    }

    std::string weights_label;
    if (!(file >> weights_label) || weights_label != "weights") {
        loaded_ = false;
        loaded_path_.clear();
        return false;
    }

    Network network{};
    network.bias = bias_value;
    for (double& weight : network.weights) {
        if (!(file >> weight)) {
            loaded_ = false;
            loaded_path_.clear();
            return false;
        }
    }

    loaded_ = true;
    network_ = network;
    loaded_path_ = path;
    return true;
}

int Evaluator::eval_cp(const engine::Board& board) const {
    if (!loaded_) {
        return board.nnue_accumulator().evaluate(board.white_to_move());
    }

    auto features = compute_features(board);
    double score = network_.bias;
    for (std::size_t i = 0; i < Network::kFeatureCount; ++i) {
        score += network_.weights[i] * features[i];
    }
    score = std::clamp(score, -30000.0, 30000.0);
    int rounded = static_cast<int>(std::round(score));
    return board.white_to_move() ? rounded : -rounded;
}

std::array<double, Evaluator::Network::kFeatureCount>
Evaluator::compute_features(const engine::Board& board) const {
    std::array<double, Network::kFeatureCount> features{};

    features[0] = static_cast<double>(board.nnue_accumulator().evaluate(true));

    std::array<int, 6> white_counts{};
    std::array<int, 6> black_counts{};

    const auto& squares = board.squares();
    for (char piece : squares) {
        switch (piece) {
        case 'P': white_counts[0]++; break;
        case 'N': white_counts[1]++; break;
        case 'B': white_counts[2]++; break;
        case 'R': white_counts[3]++; break;
        case 'Q': white_counts[4]++; break;
        case 'K': white_counts[5]++; break;
        case 'p': black_counts[0]++; break;
        case 'n': black_counts[1]++; break;
        case 'b': black_counts[2]++; break;
        case 'r': black_counts[3]++; break;
        case 'q': black_counts[4]++; break;
        case 'k': black_counts[5]++; break;
        default: break;
        }
    }

    auto diff = [&](int idx) {
        return static_cast<double>(white_counts[idx] - black_counts[idx]);
    };

    features[1] = diff(0);
    features[2] = diff(1);
    features[3] = diff(2);
    features[4] = diff(3);
    features[5] = diff(4);
    features[6] = static_cast<double>((white_counts[2] >= 2 ? 1 : 0) - (black_counts[2] >= 2 ? 1 : 0));

    return features;
}

} // namespace engine::nnue

