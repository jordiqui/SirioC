#pragma once

#include <array>
#include <string>
#include <vector>

#include "sirio/evaluation.hpp"

namespace sirio::nnue {

constexpr std::size_t kPieceTypeCount = static_cast<std::size_t>(PieceType::Count);
constexpr std::size_t kFeatureCount = kPieceTypeCount * 2;

struct FeatureState {
    std::array<int, kFeatureCount> piece_counts{};
};

struct NetworkParameters {
    double bias = 0.0;
    double scale = 1.0;
    std::array<double, kFeatureCount> piece_weights{};
};

class SingleNetworkBackend : public EvaluationBackend {
public:
    SingleNetworkBackend();

    bool load(const std::string &path, std::string *error_message);

    void initialize(const Board &board) override;
    void reset(const Board &board) override;
    void push(const Board &previous, const std::optional<Move> &move,
              const Board &current) override;
    void pop() override;
    int evaluate(const Board &board) override;

    [[nodiscard]] bool is_loaded() const { return loaded_; }
    [[nodiscard]] const std::string &loaded_path() const { return path_; }

private:
    FeatureState compute_state(const Board &board) const;
    void apply_move_to_state(FeatureState &state, const Board &previous, const Move &move,
                             const Board &current);

    bool loaded_ = false;
    std::string path_;
    NetworkParameters params_{};
    std::vector<FeatureState> stack_;
};

}  // namespace sirio::nnue

