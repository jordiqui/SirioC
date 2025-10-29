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
    return color_index(color) * static_cast<int>(kPieceTypeCount) +
           static_cast<int>(type);
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
    auto &storage = stack();
    storage.clear();
    storage.push_back(compute_state(board));
    scratch_buffer() = FeatureState{};
}

void SingleNetworkBackend::reset(const Board &board) { initialize(board); }

void SingleNetworkBackend::push(const Board &previous, const std::optional<Move> &move,
                                const Board &current) {
    auto &storage = stack();
    if (storage.empty()) {
        storage.push_back(compute_state(previous));
    }
    FeatureState &buffer = scratch_buffer();
    buffer = storage.back();
    if (move.has_value()) {
        apply_move_to_state(buffer, previous, *move, current);
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
#if defined(__GNUC__) || defined(__clang__)
    __builtin_prefetch(state.piece_counts.data(), 0, 3);
    __builtin_prefetch(params_.piece_weights.data(), 0, 3);
#elif defined(_MSC_VER)
    _mm_prefetch(reinterpret_cast<const char *>(state.piece_counts.data()), _MM_HINT_T0);
    _mm_prefetch(reinterpret_cast<const char *>(params_.piece_weights.data()), _MM_HINT_T0);
#endif

    double value = params_.bias;
#if defined(SIRIO_USE_AVX512)
    alignas(64) double accum_buffer[8];
    alignas(32) double tail_buffer[4];

    const double *weights = params_.piece_weights.data();
    const int *counts = state.piece_counts.data();

    __m256i count_lo = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(counts));
    __m512d count_lo_pd = _mm512_cvtepi32_pd(count_lo);
    __m512d weight_lo = _mm512_loadu_pd(weights);
    __m512d prod_lo = _mm512_mul_pd(weight_lo, count_lo_pd);
    _mm512_storeu_pd(accum_buffer, prod_lo);

    __m128i count_hi = _mm_loadu_si128(reinterpret_cast<const __m128i *>(counts + 8));
    __m256d count_hi_pd = _mm256_cvtepi32_pd(count_hi);
    __m256d weight_hi = _mm256_loadu_pd(weights + 8);
    __m256d prod_hi = _mm256_mul_pd(weight_hi, count_hi_pd);
    _mm256_storeu_pd(tail_buffer, prod_hi);

    for (double entry : accum_buffer) {
        value += entry;
    }
    for (double entry : tail_buffer) {
        value += entry;
    }
#elif defined(SIRIO_USE_AVX2)
    const double *weights = params_.piece_weights.data();
    const int *counts = state.piece_counts.data();
    __m256d accum = _mm256_setzero_pd();
    for (std::size_t index = 0; index < state.piece_counts.size(); index += 4) {
        __m128i count_vec = _mm_loadu_si128(reinterpret_cast<const __m128i *>(counts + index));
        __m256d count_pd = _mm256_cvtepi32_pd(count_vec);
        __m256d weight_vec = _mm256_loadu_pd(weights + index);
        accum = _mm256_fmadd_pd(weight_vec, count_pd, accum);
    }
    alignas(32) double buffer[4];
    _mm256_storeu_pd(buffer, accum);
    value += buffer[0] + buffer[1] + buffer[2] + buffer[3];
#else
    for (std::size_t index = 0; index < state.piece_counts.size(); ++index) {
        value += params_.piece_weights[index] * static_cast<double>(state.piece_counts[index]);
    }
#endif
    value *= params_.scale;
    return static_cast<int>(std::lround(value));
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

void MultiNetworkBackend::push(const Board &previous, const std::optional<Move> &move,
                               const Board &current) {
    if (primary_) {
        primary_->push(previous, move, current);
    }
    if (secondary_) {
        secondary_->push(previous, move, current);
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

