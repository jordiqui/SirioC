#include "tt.h"

#include "engine/types.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace engine {

namespace {
constexpr int kMateThreshold = 29000;
constexpr int kInfiniteScore = 32000;
constexpr uint64_t kMoveMask = (1ULL << 24) - 1ULL;
constexpr uint64_t kDepthMask = 0xFFULL;
constexpr uint64_t kFlagMask = 0x3ULL;
constexpr uint64_t kScoreMask = 0xFFFFULL;
constexpr uint64_t kGenerationMask = 0xFFULL;

inline uint64_t make_cluster_index(uint64_t key, std::size_t mask) {
    return (key & mask) * TranspositionTable::kClusterSize;
}
} // namespace

void TranspositionTable::resize(std::size_t megabytes) {
    std::size_t bytes = std::max<std::size_t>(1, megabytes) * 1024ULL * 1024ULL;
    std::size_t cluster_bytes = sizeof(Entry) * static_cast<std::size_t>(kClusterSize);
    std::size_t clusters = bytes / cluster_bytes;
    if (clusters == 0) {
        clusters = 1;
    }
    std::size_t pow2 = 1;
    while (pow2 < clusters) {
        pow2 <<= 1;
        if (pow2 == 0) {
            pow2 = clusters;
            break;
        }
    }
    entry_count_ = pow2 * static_cast<std::size_t>(kClusterSize);
    if (entry_count_ == 0) {
        entries_.reset();
        cluster_mask_ = 0;
        generation_.store(0, std::memory_order_relaxed);
        return;
    }
    entries_ = std::make_unique<Entry[]>(entry_count_);
    cluster_mask_ = pow2 - 1;
    clear();
}

void TranspositionTable::clear() {
    if (!entries_) return;
    for (std::size_t i = 0; i < entry_count_; ++i) {
        entries_[i].key_eval.store(0ULL, std::memory_order_relaxed);
        entries_[i].data.store(0ULL, std::memory_order_relaxed);
    }
    generation_.store(0, std::memory_order_relaxed);
}

void TranspositionTable::new_search() {
    generation_.fetch_add(1, std::memory_order_relaxed);
}

uint16_t TranspositionTable::pack_score(int score) {
    score = std::clamp(score, -kInfiniteScore, kInfiniteScore);
    return static_cast<uint16_t>(score + kInfiniteScore);
}

int TranspositionTable::unpack_score(uint16_t packed) {
    return static_cast<int>(packed) - kInfiniteScore;
}

uint32_t TranspositionTable::encode_move(Move move) {
    if (move == MOVE_NONE) return 0;
    uint32_t from = static_cast<uint32_t>(move_from(move));
    uint32_t to = static_cast<uint32_t>(move_to(move));
    uint32_t promo = static_cast<uint32_t>(move_promo(move));
    uint32_t flags = 0;
    if (move_is_capture(move)) flags |= 1u << 0;
    if (move_is_double_pawn(move)) flags |= 1u << 1;
    if (move_is_enpassant(move)) flags |= 1u << 2;
    if (move_is_castling(move)) flags |= 1u << 3;
    return from | (to << 6) | (promo << 12) | (flags << 15);
}

Move TranspositionTable::decode_move(uint32_t packed) {
    if (packed == 0) return MOVE_NONE;
    int from = static_cast<int>(packed & 0x3Fu);
    int to = static_cast<int>((packed >> 6) & 0x3Fu);
    int promo = static_cast<int>((packed >> 12) & 0x7u);
    uint32_t flags = (packed >> 15) & 0xFu;
    bool capture = (flags & (1u << 0)) != 0;
    bool double_pawn = (flags & (1u << 1)) != 0;
    bool enpassant = (flags & (1u << 2)) != 0;
    bool castling = (flags & (1u << 3)) != 0;
    return make_move(from, to, promo, capture, double_pawn, enpassant, castling);
}

bool TranspositionTable::probe(uint64_t key, int depth, int alpha, int beta, int ply,
                               Move& move, int& score, int& stored_depth, int& flag,
                               int& eval) const {
    if (entry_count_ == 0 || !entries_) {
        move = MOVE_NONE;
        stored_depth = -1;
        flag = 0;
        eval = 0;
        return false;
    }

    uint64_t partial = key >> 16;
    uint64_t base = make_cluster_index(key, cluster_mask_);
    const Entry* cluster = entries_.get() + base;

    for (int i = 0; i < kClusterSize; ++i) {
        uint64_t key_eval = cluster[i].key_eval.load(std::memory_order_acquire);
        if ((key_eval >> 16) != partial) continue;

        uint64_t data = cluster[i].data.load(std::memory_order_relaxed);
        move = decode_move(static_cast<uint32_t>(data & kMoveMask));
        stored_depth = static_cast<int>((data >> 24) & kDepthMask);
        flag = static_cast<int>((data >> 32) & kFlagMask);
        uint16_t packed_score = static_cast<uint16_t>((data >> 34) & kScoreMask);
        uint16_t packed_eval = static_cast<uint16_t>(key_eval & kScoreMask);
        uint8_t stored_generation = static_cast<uint8_t>((data >> 50) & kGenerationMask);
        (void)stored_generation;

        score = unpack_score(packed_score);
        eval = unpack_score(packed_eval);
        if (score > kMateThreshold) score -= ply;
        else if (score < -kMateThreshold) score += ply;

        if (stored_depth >= depth) {
            if (flag == 0 /* TT_EXACT */) return true;
            if (flag == 1 /* TT_LOWER */ && score >= beta) return true;
            if (flag == 2 /* TT_UPPER */ && score <= alpha) return true;
        }
        return false;
    }

    move = MOVE_NONE;
    stored_depth = -1;
    flag = 0;
    eval = 0;
    return false;
}

void TranspositionTable::store(uint64_t key, Move move, int depth, int score, int flag,
                               int ply, int eval) {
    if (entry_count_ == 0 || !entries_) return;

    if (score > kMateThreshold) score += ply;
    else if (score < -kMateThreshold) score -= ply;

    uint64_t partial = key >> 16;
    uint64_t key_eval = (partial << 16) | static_cast<uint64_t>(pack_score(eval));
    uint64_t base = make_cluster_index(key, cluster_mask_);
    Entry* cluster = entries_.get() + base;

    uint8_t generation = generation_.load(std::memory_order_relaxed);
    int replace_index = 0;
    int best_score = std::numeric_limits<int>::max();

    for (int i = 0; i < kClusterSize; ++i) {
        uint64_t stored_key_eval = cluster[i].key_eval.load(std::memory_order_relaxed);
        if ((stored_key_eval >> 16) == partial) {
            replace_index = i;
            break;
        }
        uint64_t data = cluster[i].data.load(std::memory_order_relaxed);
        int stored_depth = static_cast<int>((data >> 24) & kDepthMask);
        uint8_t stored_generation = static_cast<uint8_t>((data >> 50) & kGenerationMask);
        int age_penalty = (generation - stored_generation) & 0xFF;
        int score_depth = stored_depth - age_penalty;
        if (score_depth < best_score) {
            best_score = score_depth;
            replace_index = i;
        }
    }

    uint64_t packed_move = static_cast<uint64_t>(encode_move(move));
    uint64_t packed_depth = static_cast<uint64_t>(std::clamp(depth, 0, 255));
    uint64_t packed_flag = static_cast<uint64_t>(flag & 0x3);
    uint64_t packed_score = static_cast<uint64_t>(pack_score(score));

    uint64_t data = packed_move & kMoveMask;
    data |= (packed_depth & kDepthMask) << 24;
    data |= (packed_flag & kFlagMask) << 32;
    data |= (packed_score & kScoreMask) << 34;
    data |= (static_cast<uint64_t>(generation) & kGenerationMask) << 50;

    cluster[replace_index].data.store(data, std::memory_order_relaxed);
    cluster[replace_index].key_eval.store(key_eval, std::memory_order_release);
}

Move TranspositionTable::probe_move(uint64_t key) const {
    if (entry_count_ == 0 || !entries_) return MOVE_NONE;

    uint64_t partial = key >> 16;
    uint64_t base = make_cluster_index(key, cluster_mask_);
    const Entry* cluster = entries_.get() + base;
    for (int i = 0; i < kClusterSize; ++i) {
        uint64_t key_eval = cluster[i].key_eval.load(std::memory_order_acquire);
        if ((key_eval >> 16) != partial) continue;
        uint64_t data = cluster[i].data.load(std::memory_order_relaxed);
        return decode_move(static_cast<uint32_t>(data & kMoveMask));
    }
    return MOVE_NONE;
}

int TranspositionTable::hashfull() const {
    if (entry_count_ == 0 || !entries_) return 0;

    constexpr std::size_t kSampleTarget = 1000;
    std::size_t sample = std::min<std::size_t>(entry_count_, kSampleTarget);
    if (sample == 0) return 0;

    uint8_t current_generation = generation_.load(std::memory_order_relaxed);
    std::size_t filled = 0;

    for (std::size_t i = 0; i < sample; ++i) {
        std::size_t index = (i * entry_count_) / sample;
        if (index >= entry_count_) index = entry_count_ - 1;

        uint64_t data = entries_[index].data.load(std::memory_order_relaxed);
        if (data == 0) continue;

        uint8_t stored_generation = static_cast<uint8_t>((data >> 50) & kGenerationMask);
        uint8_t age = static_cast<uint8_t>((current_generation - stored_generation) & 0xFF);
        if (age <= 2) {
            ++filled;
        }
    }

    int result = static_cast<int>((filled * 1000 + sample / 2) / sample);
    if (result > 1000) result = 1000;
    return result;
}

} // namespace engine

