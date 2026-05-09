#include "sirio/nnue/backend.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

#if defined(SIRIO_USE_AVX2) || defined(SIRIO_USE_AVX512)
#include <immintrin.h>
#endif

#include "sirio/move.hpp"
#include "sirio/nnue/features.hpp"

namespace sirio::nnue {

namespace {
constexpr int max_feature_value = 64;

int color_index(Color color) { return color == Color::White ? 0 : 1; }

int feature_offset(Color color, PieceType type) {
    return color_index(color) * static_cast<int>(kPieceTypeCount) + static_cast<int>(type);
}

void clamp_non_negative(int &value) {
    if (value < 0) {
        value = 0;
    }
}

int total_piece_count(const Board &board) {
    int total = 0;
    for (int color_index = 0; color_index < 2; ++color_index) {
        Color color = color_index == 0 ? Color::White : Color::Black;
        for (int type_index = 0; type_index < static_cast<int>(PieceType::Count); ++type_index) {
            total += std::popcount(board.pieces(color, static_cast<PieceType>(type_index)));
        }
    }
    return total;
}

#if defined(SIRIO_USE_AVX512)
struct WeightRegisters {
    __m512d main;
    __m256d tail;
};

WeightRegisters load_weight_registers(const double *weights) {
    WeightRegisters regs{};
    regs.main = _mm512_loadu_pd(weights);
    regs.tail = _mm256_loadu_pd(weights + 8);
    return regs;
}

double dot_product(const WeightRegisters &regs, const int *counts) {
    __m256i count_lo = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(counts));
    __m512d count_lo_pd = _mm512_cvtepi32_pd(count_lo);
    __m512d prod_lo = _mm512_mul_pd(regs.main, count_lo_pd);

    __m128i count_hi = _mm_loadu_si128(reinterpret_cast<const __m128i *>(counts + 8));
    __m256d count_hi_pd = _mm256_cvtepi32_pd(count_hi);
    __m256d prod_hi = _mm256_mul_pd(regs.tail, count_hi_pd);

    alignas(64) double accum_buffer[8];
    alignas(32) double tail_buffer[4];
    _mm512_storeu_pd(accum_buffer, prod_lo);
    _mm256_storeu_pd(tail_buffer, prod_hi);

    double sum = 0.0;
    for (double entry : accum_buffer) {
        sum += entry;
    }
    for (double entry : tail_buffer) {
        sum += entry;
    }
    return sum;
}
#elif defined(SIRIO_USE_AVX2)
struct WeightRegisters {
    __m256d first;
    __m256d second;
    __m256d third;
};

WeightRegisters load_weight_registers(const double *weights) {
    WeightRegisters regs{};
    regs.first = _mm256_loadu_pd(weights);
    regs.second = _mm256_loadu_pd(weights + 4);
    regs.third = _mm256_loadu_pd(weights + 8);
    return regs;
}

double dot_product(const WeightRegisters &regs, const int *counts) {
    __m128i counts0_i = _mm_loadu_si128(reinterpret_cast<const __m128i *>(counts));
    __m256d counts0 = _mm256_cvtepi32_pd(counts0_i);
    __m128i counts1_i = _mm_loadu_si128(reinterpret_cast<const __m128i *>(counts + 4));
    __m256d counts1 = _mm256_cvtepi32_pd(counts1_i);
    __m128i counts2_i = _mm_loadu_si128(reinterpret_cast<const __m128i *>(counts + 8));
    __m256d counts2 = _mm256_cvtepi32_pd(counts2_i);

    __m256d accum = _mm256_mul_pd(regs.first, counts0);
    accum = _mm256_fmadd_pd(regs.second, counts1, accum);
    accum = _mm256_fmadd_pd(regs.third, counts2, accum);

    alignas(32) double buffer[4];
    _mm256_storeu_pd(buffer, accum);
    return buffer[0] + buffer[1] + buffer[2] + buffer[3];
}
#else
struct WeightRegisters {
    const double *weights = nullptr;
};

WeightRegisters load_weight_registers(const double *weights) {
    return WeightRegisters{weights};
}

double dot_product(const WeightRegisters &regs, const int *counts) {
    double sum = 0.0;
    for (std::size_t idx = 0; idx < kFeatureCount; ++idx) {
        sum += regs.weights[idx] * static_cast<double>(counts[idx]);
    }
    return sum;
}
#endif



bool apply_sirio_nnue2_minimal_test_quantization(const Nnue2NetworkParameters &network,
                                                 std::int64_t &output_accum) {
    const std::int64_t denom =
        static_cast<std::int64_t>(network.header.quant_input_scale) * network.header.quant_output_scale;
    if (denom > 0) {
        output_accum /= denom;
    }
    return true;
}

}  // namespace


bool SparsePerspectiveState::push(SparseFeature feature) {
    if (count >= active.size()) {
        return false;
    }
    active[count] = feature;
    ++count;
    return true;
}

std::size_t SparseFeatureState::total_active_features() const {
    std::size_t total = 0;
    for (const auto &perspective : perspectives) {
        total += perspective.count;
    }
    return total;
}

bool Nnue2NetworkParameters::is_initialized() const {
    return is_valid_nnue2_header(header) && !input_weights.empty() && !hidden_bias.empty() &&
           !output_weights.empty();
}

void Nnue2NetworkParameters::clear() {
    header = Nnue2BinaryHeader{};
    input_weights.clear();
    hidden_bias.clear();
    output_weights.clear();
    output_bias = 0;
}

bool is_valid_nnue2_header(const Nnue2BinaryHeader &header) {
    constexpr std::array<char, 12> expected_magic = {'S', 'i', 'r', 'i', 'o', 'N', 'N',
                                                     'U', 'E', '2', '\0', '\0'};
    return header.magic == expected_magic && header.version == 2 &&
           header.feature_set_id == 1 &&
           header.features_per_perspective == kSirioHalfKAv1FeaturesPerPerspective &&
           header.perspective_count == kNnue2PerspectiveCount &&
           header.accumulator_size == kNnue2AccumulatorSize && header.hidden_dimensions > 0 &&
           header.output_dimensions == 1 &&
           header.payload_bytes ==
               header.input_weights_bytes + header.hidden_bias_bytes + header.output_weights_bytes +
                   header.output_bias_bytes;
}

Nnue2BinaryHeader make_default_nnue2_header() {
    Nnue2BinaryHeader header{};
    header.magic = {'S', 'i', 'r', 'i', 'o', 'N', 'N', 'U', 'E', '2', '\0', '\0'};
    header.version = 2;
    header.feature_set_id = 1;
    header.flags = 0;
    header.features_per_perspective = kSirioHalfKAv1FeaturesPerPerspective;
    header.perspective_count = kNnue2PerspectiveCount;
    header.accumulator_size = kNnue2AccumulatorSize;
    header.hidden_dimensions = static_cast<std::uint32_t>(kNnue2AccumulatorSize);
    header.output_dimensions = 1;
    header.quant_input_scale = 256;
    header.quant_output_scale = 256;
    header.input_weights_bytes = kSirioHalfKAv1FeaturesPerPerspective * kNnue2AccumulatorSize *
                                 sizeof(std::int16_t);
    header.hidden_bias_bytes = kNnue2AccumulatorSize * sizeof(std::int16_t);
    header.output_weights_bytes = kNnue2AccumulatorSize * sizeof(std::int16_t);
    header.output_bias_bytes = sizeof(std::int32_t);
    header.payload_bytes = header.input_weights_bytes + header.hidden_bias_bytes +
                           header.output_weights_bytes + header.output_bias_bytes;
    return header;
}

bool load_nnue2_network_file(const std::string &path, Nnue2NetworkParameters &out_network,
                             std::string &error_message) {
    out_network.clear();
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error_message = "Unable to open SirioNNUE2 file: " + path;
        return false;
    }

    Nnue2BinaryHeader header{};
    input.read(reinterpret_cast<char *>(&header), static_cast<std::streamsize>(sizeof(header)));
    if (input.gcount() != static_cast<std::streamsize>(sizeof(header))) {
        error_message = "Truncated SirioNNUE2 header";
        return false;
    }
    if (!is_valid_nnue2_header(header)) {
        error_message = "Invalid SirioNNUE2 header contract";
        return false;
    }

    const std::size_t input_elems = header.input_weights_bytes / sizeof(std::int16_t);
    const std::size_t hidden_elems = header.hidden_bias_bytes / sizeof(std::int16_t);
    const std::size_t output_elems = header.output_weights_bytes / sizeof(std::int16_t);
    out_network.header = header;
    out_network.input_weights.resize(input_elems);
    out_network.hidden_bias.resize(hidden_elems);
    out_network.output_weights.resize(output_elems);

    input.read(reinterpret_cast<char *>(out_network.input_weights.data()),
               static_cast<std::streamsize>(header.input_weights_bytes));
    input.read(reinterpret_cast<char *>(out_network.hidden_bias.data()),
               static_cast<std::streamsize>(header.hidden_bias_bytes));
    input.read(reinterpret_cast<char *>(out_network.output_weights.data()),
               static_cast<std::streamsize>(header.output_weights_bytes));
    input.read(reinterpret_cast<char *>(&out_network.output_bias),
               static_cast<std::streamsize>(header.output_bias_bytes));
    if (!input) {
        out_network.clear();
        error_message = "Truncated SirioNNUE2 payload";
        return false;
    }
    char extra = 0;
    if (input.read(&extra, 1)) {
        out_network.clear();
        error_message = "Unexpected trailing bytes in SirioNNUE2 file";
        return false;
    }
    return true;
}

bool decode_nnue2_minimal_layout(const Nnue2NetworkParameters &network,
                                 Nnue2MinimalDecodedLayout &out_layout,
                                 std::string &error_message) {
    if (!network.is_initialized()) {
        error_message = "SirioNNUE2 network is not initialized";
        return false;
    }

    const auto &header = network.header;
    out_layout.features_per_perspective = header.features_per_perspective;
    out_layout.accumulator_size = header.accumulator_size;
    out_layout.hidden1_size = header.hidden_dimensions;
    out_layout.hidden2_size = 0;
    out_layout.output_size = header.output_dimensions;

    if (header.version != 2 || header.feature_set_id != 1) {
        error_message = "Unsupported SirioNNUE2 metadata contract";
        return false;
    }
    if (out_layout.features_per_perspective != kSirioHalfKAv1FeaturesPerPerspective ||
        out_layout.accumulator_size != kNnue2AccumulatorSize ||
        out_layout.hidden1_size != kNnue2AccumulatorSize || out_layout.output_size != 1) {
        error_message = "SirioNNUE2-MinimalV1 dimensions do not match required contract";
        return false;
    }
    if (network.input_weights.size() !=
            static_cast<std::size_t>(out_layout.features_per_perspective) * out_layout.hidden1_size ||
        network.hidden_bias.size() != out_layout.hidden1_size ||
        network.output_weights.size() != out_layout.hidden1_size) {
        error_message = "SirioNNUE2-MinimalV1 tensor payload size mismatch";
        return false;
    }
    return true;
}

bool refresh_sirio_nnue2_minimal_accumulator(
    const Board &board, const Nnue2NetworkParameters &network,
    SirioNNUE2MinimalAccumulator &accumulator, std::string &error_message) {
    accumulator.clear();
    Nnue2MinimalDecodedLayout layout{};
    if (!decode_nnue2_minimal_layout(network, layout, error_message)) {
        return false;
    }

    SparseFeatureState sparse{};
    if (!encode_sirio_halfka_v1(board, sparse)) {
        error_message = "Failed to encode SirioHalfKAv1 features";
        return false;
    }

    accumulator.hidden_pre_activation.assign(layout.hidden1_size, 0);
    for (std::size_t h = 0; h < layout.hidden1_size; ++h) {
        accumulator.hidden_pre_activation[h] = network.hidden_bias[h];
    }

    for (std::size_t perspective = 0; perspective < kNnue2PerspectiveCount; ++perspective) {
        const auto &perspective_state = sparse.perspectives[perspective];
        for (std::size_t idx = 0; idx < perspective_state.count; ++idx) {
            const auto feature = perspective_state.active[idx];
            if (feature.index >= layout.features_per_perspective) {
                error_message = "Feature index exceeds SirioHalfKAv1 dimensions";
                accumulator.clear();
                return false;
            }
            const std::size_t row_offset = static_cast<std::size_t>(feature.index) * layout.hidden1_size;
            for (std::size_t h = 0; h < layout.hidden1_size; ++h) {
                accumulator.hidden_pre_activation[h] +=
                    static_cast<std::int32_t>(network.input_weights[row_offset + h]) *
                    static_cast<std::int32_t>(feature.value);
            }
        }
    }
    accumulator.valid = true;
    return true;
}

bool evaluate_sirio_nnue2_minimal_accumulator(
    const SirioNNUE2MinimalAccumulator &accumulator, const Nnue2NetworkParameters &network,
    std::int32_t &out_score, std::string &error_message) {
    out_score = 0;
    Nnue2MinimalDecodedLayout layout{};
    if (!decode_nnue2_minimal_layout(network, layout, error_message)) {
        return false;
    }
    if (!accumulator.valid) {
        error_message = "SirioNNUE2-MinimalV1 accumulator is not valid";
        return false;
    }
    if (accumulator.hidden_pre_activation.size() != layout.hidden1_size) {
        error_message = "SirioNNUE2-MinimalV1 accumulator dimension mismatch";
        return false;
    }

    std::int64_t output_accum = network.output_bias;
    for (std::size_t h = 0; h < layout.hidden1_size; ++h) {
        const std::int32_t activated = std::max<std::int32_t>(0, accumulator.hidden_pre_activation[h]);
        output_accum += static_cast<std::int64_t>(activated) * network.output_weights[h];
    }
    apply_sirio_nnue2_minimal_test_quantization(network, output_accum);
    out_score = static_cast<std::int32_t>(output_accum);
    return true;
}

bool evaluate_loaded_nnue2_minimal_v1(const Board &board, const Nnue2NetworkParameters &network,
                                      std::int32_t &out_score, std::string &error_message) {
    SirioNNUE2MinimalAccumulator accumulator{};
    if (!refresh_sirio_nnue2_minimal_accumulator(board, network, accumulator, error_message)) {
        out_score = 0;
        return false;
    }
    return evaluate_sirio_nnue2_minimal_accumulator(accumulator, network, out_score, error_message);
}

bool evaluate_loaded_nnue2_minimal_v1_probe_white_pov(
    const Board &board, const Nnue2NetworkParameters &network, std::int32_t &out_white_pov_score,
    std::string &error_message) {
    // P0-12 contract: this non-default probe returns White-POV directly and does not
    // apply side-to-move sign normalization.
    return evaluate_loaded_nnue2_minimal_v1(board, network, out_white_pov_score, error_message);
}

bool evaluate_loaded_nnue2_minimal_v1_probe_stm_pov(
    const Board &board, const Nnue2NetworkParameters &network, std::int32_t &out_stm_pov_score,
    std::string &error_message) {
    std::int32_t white_pov_score = 0;
    if (!evaluate_loaded_nnue2_minimal_v1_probe_white_pov(board, network, white_pov_score,
                                                           error_message)) {
        out_stm_pov_score = 0;
        return false;
    }

    // P0-13 contract: non-default adapter converts White-POV probe output to side-to-move POV.
    out_stm_pov_score =
        board.side_to_move() == Color::White ? white_pov_score : -white_pov_score;
    return true;
}

ExperimentalEvalRoutingResult route_experimental_nnue2_evaluation(
    const Board &board, ExperimentalEvalBackend backend, std::int32_t classical_score,
    const Nnue2NetworkParameters *network, std::string *diagnostic_message) {
    ExperimentalEvalRoutingResult result{};
    result.score = classical_score;

    if (backend != ExperimentalEvalBackend::ExperimentalSirioNNUE2) {
        if (diagnostic_message) {
            *diagnostic_message = "Experimental backend disabled; classical evaluation preserved";
        }
        return result;
    }

    if (!network) {
        result.fell_back_to_classical = true;
        if (diagnostic_message) {
            *diagnostic_message =
                "Experimental SirioNNUE2 backend selected but no network provided; using classical fallback";
        }
        return result;
    }

    std::string nnue_error;
    std::int32_t routed_score = 0;
    if (!evaluate_loaded_nnue2_minimal_v1_probe_stm_pov(board, *network, routed_score,
                                                        nnue_error)) {
        result.fell_back_to_classical = true;
        if (diagnostic_message) {
            *diagnostic_message = "Experimental SirioNNUE2 backend selected but network rejected: " +
                                  nnue_error + "; using classical fallback";
        }
        return result;
    }

    result.score = routed_score;
    result.used_experimental_backend = true;
    if (diagnostic_message) {
        *diagnostic_message = "Experimental SirioNNUE2 backend active via STM-POV adapter";
    }
    return result;
}

SparseFeatureState compute_sparse_feature_state(const Board &board) {
    SparseFeatureState state{};
    for (int perspective = 0; perspective < 2; ++perspective) {
        const Color self = perspective == 0 ? Color::White : Color::Black;
        const Color opp = opposite(self);
        for (int color_idx = 0; color_idx < 2; ++color_idx) {
            const Color piece_color = color_idx == 0 ? self : opp;
            for (std::size_t type_idx = 0; type_idx < kPieceTypeCount; ++type_idx) {
                const PieceType type = static_cast<PieceType>(type_idx);
                const int count = static_cast<int>(std::popcount(board.pieces(piece_color, type)));
                const std::uint32_t base = static_cast<std::uint32_t>(color_idx * kPieceTypeCount + type_idx);
                for (int n = 0; n < count; ++n) {
                    if (!state.perspectives[perspective].push(SparseFeature{base, 1})) {
                        break;
                    }
                }
            }
        }
    }
    return state;
}

void incremental_update_sparse_state(SparseFeatureState &state, const Board &, const Move &,
                                     Color) {
    // Foundation contract: keep sparse state well-defined and safe by recomputing from board
    // in later steps. For now this marks state as non-incremental placeholder if saturated.
    if (state.total_active_features() >
        kNnue2PerspectiveCount * kNnue2MaxActiveFeatures) {
        state.clear();
    }
}

void refresh_accumulators(const SparseFeatureState &state, Nnue2AccumulatorPair &accumulators,
                          const Nnue2NetworkParameters &network) {
    accumulators.clear();
    if (!network.is_initialized()) {
        return;
    }
    for (std::size_t perspective = 0; perspective < kNnue2PerspectiveCount; ++perspective) {
        accumulators.perspectives[perspective].valid = true;
        const auto count = std::min<std::size_t>(state.perspectives[perspective].count,
                                                 accumulators.perspectives[perspective].values.size());
        for (std::size_t i = 0; i < count; ++i) {
            accumulators.perspectives[perspective].values[i] =
                static_cast<std::int16_t>(state.perspectives[perspective].active[i].index);
        }
    }
}

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

std::unique_ptr<EvaluationBackend> SingleNetworkBackend::clone() const {
    auto copy = std::make_unique<SingleNetworkBackend>();
    copy->loaded_ = loaded_;
    copy->path_ = path_;
    copy->params_ = params_;
    return copy;
}

void SingleNetworkBackend::set_thread_accumulator(ThreadAccumulator *accumulator) {
    thread_accumulator_ = accumulator;
    if (thread_accumulator_) {
        thread_accumulator_->reset();
    }
}

FeatureState SingleNetworkBackend::extract_features(const Board &board) const {
    return compute_state(board);
}

int SingleNetworkBackend::evaluate_state(const FeatureState &state) const {
    if (!loaded_) {
        return 0;
    }

#if defined(__GNUC__) || defined(__clang__)
    __builtin_prefetch(state.piece_counts.data(), 0, 3);
    __builtin_prefetch(params_.piece_weights.data(), 0, 3);
#elif defined(_MSC_VER)
    _mm_prefetch(reinterpret_cast<const char *>(state.piece_counts.data()), _MM_HINT_T0);
    _mm_prefetch(reinterpret_cast<const char *>(params_.piece_weights.data()), _MM_HINT_T0);
#endif

    const WeightRegisters regs = load_weight_registers(params_.piece_weights.data());
    double value = params_.bias + dot_product(regs, state.piece_counts.data());
    value *= params_.scale;
    return static_cast<int>(std::lround(value));
}

void SingleNetworkBackend::evaluate_batch(std::span<const FeatureState> states, std::span<int> out) const {
    if (states.size() != out.size()) {
        throw std::invalid_argument("evaluate_batch requires matching state/output spans");
    }

    if (!loaded_) {
        std::fill(out.begin(), out.end(), 0);
        return;
    }

    const WeightRegisters regs = load_weight_registers(params_.piece_weights.data());
    for (std::size_t index = 0; index < states.size(); ++index) {
        const FeatureState &state = states[index];
#if defined(__GNUC__) || defined(__clang__)
        __builtin_prefetch(state.piece_counts.data(), 0, 2);
#elif defined(_MSC_VER)
        _mm_prefetch(reinterpret_cast<const char *>(state.piece_counts.data()), _MM_HINT_T0);
#endif
        double value = params_.bias + dot_product(regs, state.piece_counts.data());
        value *= params_.scale;
        out[index] = static_cast<int>(std::lround(value));
    }
}

std::vector<FeatureState> &SingleNetworkBackend::stack() {
    if (thread_accumulator_) {
        return thread_accumulator_->stack;
    }
    return stack_;
}

const std::vector<FeatureState> &SingleNetworkBackend::stack() const {
    if (thread_accumulator_) {
        return thread_accumulator_->stack;
    }
    return stack_;
}

FeatureState &SingleNetworkBackend::scratch_buffer() {
    if (thread_accumulator_) {
        return thread_accumulator_->scratch;
    }
    return scratch_;
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

void SingleNetworkBackend::apply_move_to_state(FeatureState &state, Color mover,
                                               const Move &move, const Board &) {
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
    auto &storage = stack();
    storage.clear();
    storage.push_back(compute_state(board));
    scratch_buffer() = FeatureState{};
}

void SingleNetworkBackend::reset(const Board &board) { initialize(board); }

void SingleNetworkBackend::push(const Board &current, const std::optional<Move> &move,
                                Color mover) {
    auto &storage = stack();
    if (storage.empty()) {
        storage.push_back(compute_state(current));
    }
    FeatureState &buffer = scratch_buffer();
    buffer = storage.back();
    if (move.has_value()) {
        apply_move_to_state(buffer, mover, *move, current);
    }
    storage.push_back(buffer);
}

void SingleNetworkBackend::pop() {
    auto &storage = stack();
    if (storage.size() > 1) {
        storage.pop_back();
    }
}

int SingleNetworkBackend::evaluate(const Board &board) {
    auto &storage = stack();
    if (storage.empty()) {
        storage.push_back(compute_state(board));
    }
    if (!loaded_) {
        return 0;
    }
    std::size_t expected = board.history().size();
    if (storage.size() != expected) {
        storage.clear();
        storage.push_back(compute_state(board));
    }
    const FeatureState &state = storage.back();
    return evaluate_state(state);
}

MultiNetworkBackend::MultiNetworkBackend(std::unique_ptr<SingleNetworkBackend> primary,
                                         std::unique_ptr<SingleNetworkBackend> secondary,
                                         NetworkSelectionPolicy policy, int phase_threshold)
    : primary_(std::move(primary)),
      secondary_(std::move(secondary)),
      policy_(policy),
      phase_threshold_(phase_threshold) {}

std::unique_ptr<EvaluationBackend> MultiNetworkBackend::clone() const {
    auto primary_copy = primary_ ? std::make_unique<SingleNetworkBackend>(*primary_) : nullptr;
    std::unique_ptr<SingleNetworkBackend> secondary_copy = nullptr;
    if (secondary_) {
        secondary_copy = std::make_unique<SingleNetworkBackend>(*secondary_);
    }
    return std::make_unique<MultiNetworkBackend>(std::move(primary_copy), std::move(secondary_copy),
                                                 policy_, phase_threshold_);
}

void MultiNetworkBackend::set_thread_accumulators(ThreadAccumulator *primary,
                                                  ThreadAccumulator *secondary) {
    if (primary_) {
        primary_->set_thread_accumulator(primary);
    }
    if (secondary_) {
        secondary_->set_thread_accumulator(secondary);
    }
}

void MultiNetworkBackend::initialize(const Board &board) {
    if (primary_) {
        primary_->initialize(board);
    }
    if (secondary_) {
        secondary_->initialize(board);
    }
    ply_ = 0;
}

void MultiNetworkBackend::reset(const Board &board) { initialize(board); }

void MultiNetworkBackend::push(const Board &current, const std::optional<Move> &move,
                               Color mover) {
    if (primary_) {
        primary_->push(current, move, mover);
    }
    if (secondary_) {
        secondary_->push(current, move, mover);
    }
    ++ply_;
}

void MultiNetworkBackend::pop() {
    if (primary_) {
        primary_->pop();
    }
    if (secondary_) {
        secondary_->pop();
    }
    if (ply_ > 0) {
        --ply_;
    }
}

SingleNetworkBackend *MultiNetworkBackend::active_backend(const Board &board) {
    return const_cast<SingleNetworkBackend *>(
        static_cast<const MultiNetworkBackend &>(*this).active_backend(board));
}

const SingleNetworkBackend *MultiNetworkBackend::active_backend(const Board &board) const {
    if (!primary_ || !primary_->is_loaded()) {
        return nullptr;
    }
    if (!secondary_ || !secondary_->is_loaded()) {
        return primary_.get();
    }
    if (phase_threshold_ <= 0) {
        return primary_.get();
    }

    switch (policy_) {
        case NetworkSelectionPolicy::Material: {
            int total_material = total_piece_count(board);
            if (total_material <= phase_threshold_) {
                return secondary_.get();
            }
            break;
        }
        case NetworkSelectionPolicy::Depth: {
            if (ply_ >= phase_threshold_) {
                return secondary_.get();
            }
            break;
        }
    }
    return primary_.get();
}

int MultiNetworkBackend::evaluate(const Board &board) {
    if (SingleNetworkBackend *backend = active_backend(board)) {
        return backend->evaluate(board);
    }
    return 0;
}

}  // namespace sirio::nnue
