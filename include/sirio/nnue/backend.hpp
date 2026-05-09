#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "sirio/evaluation.hpp"

namespace sirio::nnue {

constexpr std::size_t kPieceTypeCount = static_cast<std::size_t>(PieceType::Count);
constexpr std::size_t kFeatureCount = kPieceTypeCount * 2;
constexpr std::size_t kNnue2PerspectiveCount = 2;
constexpr std::size_t kNnue2MaxActiveFeatures = 64;
constexpr std::size_t kNnue2AccumulatorSize = 256;

struct FeatureState {
    std::array<int, kFeatureCount> piece_counts{};
};

struct SparseFeature {
    std::uint32_t index = 0;
    std::int16_t value = 1;
};

struct SparsePerspectiveState {
    std::array<SparseFeature, kNnue2MaxActiveFeatures> active{};
    std::uint16_t count = 0;

    void clear() {
        count = 0;
        active.fill(SparseFeature{});
    }

    [[nodiscard]] bool push(SparseFeature feature);
};

struct SparseFeatureState {
    std::array<SparsePerspectiveState, kNnue2PerspectiveCount> perspectives{};

    void clear() {
        for (auto &perspective : perspectives) {
            perspective.clear();
        }
    }

    [[nodiscard]] std::size_t total_active_features() const;
};

struct Nnue2Accumulator {
    std::array<std::int16_t, kNnue2AccumulatorSize> values{};
    bool valid = false;

    void clear() {
        values.fill(0);
        valid = false;
    }
};

struct Nnue2AccumulatorPair {
    std::array<Nnue2Accumulator, kNnue2PerspectiveCount> perspectives{};

    void clear() {
        for (auto &perspective : perspectives) {
            perspective.clear();
        }
    }
};

struct ThreadAccumulator {
    std::vector<FeatureState> stack;
    FeatureState scratch{};

    void reset() {
        stack.clear();
        scratch = FeatureState{};
    }
};

struct NetworkParameters {
    double bias = 0.0;
    double scale = 1.0;
    std::array<double, kFeatureCount> piece_weights{};
};

struct Nnue2BinaryHeader {
    std::array<char, 12> magic{};
    std::uint16_t version = 0;
    std::uint16_t feature_set_id = 0;
    std::uint16_t flags = 0;
    std::uint32_t features_per_perspective = 0;
    std::uint32_t perspective_count = 0;
    std::uint32_t accumulator_size = 0;
    std::uint32_t hidden_dimensions = 0;
    std::uint32_t output_dimensions = 0;
    std::uint32_t quant_input_scale = 0;
    std::uint32_t quant_output_scale = 0;
    std::uint32_t input_weights_bytes = 0;
    std::uint32_t hidden_bias_bytes = 0;
    std::uint32_t output_weights_bytes = 0;
    std::uint32_t output_bias_bytes = 0;
    std::uint32_t payload_bytes = 0;
    std::uint32_t checksum = 0;
};

struct Nnue2NetworkParameters {
    Nnue2BinaryHeader header{};
    std::vector<std::int16_t> input_weights;
    std::vector<std::int16_t> hidden_bias;
    std::vector<std::int16_t> output_weights;
    std::int32_t output_bias = 0;

    [[nodiscard]] bool is_initialized() const;
    void clear();
};

struct Nnue2MinimalDecodedLayout {
    std::string model_layout_name = "SirioNNUE2-MinimalV1";
    std::uint32_t model_layout_version = 1;
    std::string feature_set = "SirioHalfKAv1";
    std::uint32_t features_per_perspective = 0;
    std::uint32_t accumulator_size = 0;
    std::uint32_t hidden1_size = 0;
    std::uint32_t hidden2_size = 0;
    std::uint32_t output_size = 0;
    std::string activation = "relu";
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

    void set_thread_accumulator(ThreadAccumulator *accumulator);

    [[nodiscard]] FeatureState extract_features(const Board &board) const;
    [[nodiscard]] int evaluate_state(const FeatureState &state) const;
    void evaluate_batch(std::span<const FeatureState> states, std::span<int> out) const;

    void initialize(const Board &board) override;
    void reset(const Board &board) override;
    void push(const Board &current, const std::optional<Move> &move,
              Color mover) override;
    void pop() override;
    int evaluate(const Board &board) override;

    [[nodiscard]] bool is_loaded() const { return loaded_; }
    [[nodiscard]] const std::string &loaded_path() const { return path_; }

private:
    FeatureState compute_state(const Board &board) const;
    void apply_move_to_state(FeatureState &state, Color mover, const Move &move,
                             const Board &current);
    std::vector<FeatureState> &stack();
    [[nodiscard]] const std::vector<FeatureState> &stack() const;
    FeatureState &scratch_buffer();

    bool loaded_ = false;
    std::string path_;
    NetworkParameters params_{};
    std::vector<FeatureState> stack_;
    ThreadAccumulator *thread_accumulator_ = nullptr;
    FeatureState scratch_{};
};

class MultiNetworkBackend : public EvaluationBackend {
public:
    MultiNetworkBackend(std::unique_ptr<SingleNetworkBackend> primary,
                        std::unique_ptr<SingleNetworkBackend> secondary,
                        NetworkSelectionPolicy policy, int phase_threshold);

    [[nodiscard]] std::unique_ptr<EvaluationBackend> clone() const override;

    void set_thread_accumulators(ThreadAccumulator *primary, ThreadAccumulator *secondary);

    void initialize(const Board &board) override;
    void reset(const Board &board) override;
    void push(const Board &current, const std::optional<Move> &move,
              Color mover) override;
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

[[nodiscard]] bool is_valid_nnue2_header(const Nnue2BinaryHeader &header);
[[nodiscard]] Nnue2BinaryHeader make_default_nnue2_header();
[[nodiscard]] bool load_nnue2_network_file(const std::string &path, Nnue2NetworkParameters &out_network,
                                           std::string &error_message);
[[nodiscard]] bool decode_nnue2_minimal_layout(const Nnue2NetworkParameters &network,
                                               Nnue2MinimalDecodedLayout &out_layout,
                                               std::string &error_message);
[[nodiscard]] bool evaluate_loaded_nnue2_minimal_v1(const Board &board,
                                                    const Nnue2NetworkParameters &network,
                                                    std::int32_t &out_score,
                                                    std::string &error_message);
[[nodiscard]] SparseFeatureState compute_sparse_feature_state(const Board &board);
void incremental_update_sparse_state(SparseFeatureState &state, const Board &current,
                                     const Move &move, Color mover);
void refresh_accumulators(const SparseFeatureState &state, Nnue2AccumulatorPair &accumulators,
                          const Nnue2NetworkParameters &network);

}  // namespace sirio::nnue
