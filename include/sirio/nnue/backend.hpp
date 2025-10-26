#pragma once

#include <array>
#include <memory>
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

enum class NetworkSelectionPolicy { Material, Depth };

struct MultiNetworkConfig {
    std::string primary_path;
    std::string secondary_path;
    NetworkSelectionPolicy policy = NetworkSelectionPolicy::Material;
    int phase_threshold = 0;
};

class SingleNetworkBackend : public EvaluationBackend {
public:
    SingleNetworkBackend();

    bool load(const std::string &path, std::string *error_message);

    [[nodiscard]] std::unique_ptr<EvaluationBackend> clone() const override;

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

class MultiNetworkBackend : public EvaluationBackend {
public:
    MultiNetworkBackend(std::unique_ptr<SingleNetworkBackend> primary,
                        std::unique_ptr<SingleNetworkBackend> secondary,
                        NetworkSelectionPolicy policy, int phase_threshold);

    [[nodiscard]] std::unique_ptr<EvaluationBackend> clone() const override;

    void initialize(const Board &board) override;
    void reset(const Board &board) override;
    void push(const Board &previous, const std::optional<Move> &move,
              const Board &current) override;
    void pop() override;
    int evaluate(const Board &board) override;

private:
    [[nodiscard]] SingleNetworkBackend *active_backend(const Board &board);
    [[nodiscard]] const SingleNetworkBackend *active_backend(const Board &board) const;

    std::unique_ptr<SingleNetworkBackend> primary_;
    std::unique_ptr<SingleNetworkBackend> secondary_;
    NetworkSelectionPolicy policy_;
    int phase_threshold_;
    int ply_ = 0;
};

}  // namespace sirio::nnue

