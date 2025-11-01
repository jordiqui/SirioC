#include "sirio/nnue/backend.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

#if defined(SIRIO_USE_AVX2) || defined(SIRIO_USE_AVX512)
#include <immintrin.h>
#endif

#include "sirio/move.hpp"

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

