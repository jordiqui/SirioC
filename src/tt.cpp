#include "sirio/transposition_table.hpp"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstdint>
#include <fstream>
#include <limits>
#include <mutex>
#include <string_view>

namespace sirio {
namespace {

constexpr std::size_t kDefaultTranspositionTableSizeMb = 16;

std::atomic<std::size_t> transposition_table_size_mb{kDefaultTranspositionTableSizeMb};
std::atomic<std::uint64_t> transposition_table_epoch{1};

std::uint64_t high_product(std::uint64_t lhs, std::uint64_t rhs) {
    const std::uint64_t lhs_hi = lhs >> 32U;
    const std::uint64_t lhs_lo = lhs & 0xFFFFFFFFULL;
    const std::uint64_t rhs_hi = rhs >> 32U;
    const std::uint64_t rhs_lo = rhs & 0xFFFFFFFFULL;

    const std::uint64_t hi_hi = lhs_hi * rhs_hi;
    const std::uint64_t hi_lo = lhs_hi * rhs_lo;
    const std::uint64_t lo_hi = lhs_lo * rhs_hi;
    const std::uint64_t lo_lo = lhs_lo * rhs_lo;

    const std::uint64_t mid = hi_lo + lo_hi;
    const std::uint64_t mid_low = mid << 32U;
    const std::uint64_t mid_high = mid >> 32U;

    const std::uint64_t low = lo_lo + mid_low;
    const std::uint64_t carry = low < lo_lo ? 1ULL : 0ULL;

    return hi_hi + mid_high + carry;
}

}  // namespace

std::uint8_t GlobalTranspositionTable::pack_generation(std::uint8_t generation) {
    if (generation == 0) {
        return 0;
    }
    const std::uint8_t max_generation = static_cast<std::uint8_t>(kGenerationMask >> 2);
    generation = static_cast<std::uint8_t>(((generation - 1) % max_generation) + 1);
    return static_cast<std::uint8_t>(generation << 2);
}

std::uint8_t GlobalTranspositionTable::unpack_generation(std::uint8_t gen_and_type) {
    return static_cast<std::uint8_t>((gen_and_type & kGenerationMask) >> 2);
}

std::uint8_t GlobalTranspositionTable::pack_depth(int depth) {
    const int min_depth = -kDepthOffset + 1;
    const int max_depth = static_cast<int>(std::numeric_limits<std::uint8_t>::max()) - kDepthOffset;
    int clamped = std::clamp(depth, min_depth, max_depth);
    int stored = clamped + kDepthOffset;
    if (stored <= 0) {
        stored = 1;
    }
    return static_cast<std::uint8_t>(stored);
}

int GlobalTranspositionTable::unpack_depth(std::uint8_t depth8) {
    return static_cast<int>(depth8) - kDepthOffset;
}

std::uint32_t GlobalTranspositionTable::pack_move(const Move &move) {
    constexpr std::uint32_t kNone = 0x7;
    std::uint32_t data = 0;
    data |= static_cast<std::uint32_t>(move.from & 0x3F);
    data |= static_cast<std::uint32_t>(move.to & 0x3F) << 6;
    data |= static_cast<std::uint32_t>(static_cast<int>(move.piece) & 0x7) << 12;
    std::uint32_t captured = move.captured.has_value() ? static_cast<std::uint32_t>(static_cast<int>(*move.captured) & 0x7)
                                                       : kNone;
    data |= captured << 15;
    std::uint32_t promotion =
        move.promotion.has_value() ? static_cast<std::uint32_t>(static_cast<int>(*move.promotion) & 0x7) : kNone;
    data |= promotion << 18;
    if (move.is_en_passant) {
        data |= 1u << 21;
    }
    if (move.is_castling) {
        data |= 1u << 22;
    }
    return data;
}

Move GlobalTranspositionTable::unpack_move(std::uint32_t packed_move) {
    constexpr std::uint32_t kNone = 0x7;
    Move move;
    move.from = static_cast<int>(packed_move & 0x3F);
    move.to = static_cast<int>((packed_move >> 6) & 0x3F);
    move.piece = static_cast<PieceType>((packed_move >> 12) & 0x7);
    std::uint32_t captured = (packed_move >> 15) & 0x7;
    if (captured != kNone) {
        move.captured = static_cast<PieceType>(captured);
    }
    std::uint32_t promotion = (packed_move >> 18) & 0x7;
    if (promotion != kNone) {
        move.promotion = static_cast<PieceType>(promotion);
    }
    move.is_en_passant = (packed_move >> 21) & 0x1;
    move.is_castling = (packed_move >> 22) & 0x1;
    return move;
}

int GlobalTranspositionTable::replacement_score(const PackedTTEntry &entry,
                                                std::uint8_t current_generation) {
    if (!entry.occupied()) {
        return std::numeric_limits<int>::min();
    }
    const std::uint8_t raw_age = static_cast<std::uint8_t>((kGenerationCycle + current_generation -
                                                            entry.stored_generation()) & kGenerationMask);
    const int age = static_cast<int>(raw_age >> 2);
    const int depth = std::max(unpack_depth(entry.depth8), 0);

    int score = depth * 16;
    switch (static_cast<TTNodeType>(entry.stored_type())) {
        case TTNodeType::Exact:
            score += 48;
            break;
        case TTNodeType::LowerBound:
            score += 16;
            break;
        case TTNodeType::UpperBound:
            score += 8;
            break;
    }
    if (entry.packed_move != 0) {
        score += 4;
    }
    if (entry.static_eval != 0) {
        score += 1;
    }
    score -= age * 6;
    return score;
}

std::uint8_t GlobalTranspositionTable::prepare_for_search() {
    std::unique_lock lock(global_mutex_);
    ensure_settings_locked();
    const std::uint8_t max_generation = static_cast<std::uint8_t>(kGenerationMask >> 2);
    generation_counter_ = static_cast<std::uint8_t>((generation_counter_ % max_generation) + 1);
    generation_tag_ = pack_generation(generation_counter_);
    return generation_counter_;
}

void GlobalTranspositionTable::store(std::uint64_t key, const TTEntry &entry, std::uint8_t generation) {
    std::shared_lock lock(global_mutex_);
    if (clusters_.empty()) {
        return;
    }

    const std::size_t index = cluster_index(key);
    Cluster &cluster = clusters_[index];
    const std::size_t slot_index = select_entry_index(cluster, static_cast<std::uint16_t>(key));

    const std::uint8_t generation_bits = pack_generation(generation);
    PackedTTEntry packed{};
    packed.key16 = static_cast<std::uint16_t>(key);
    packed.depth8 = pack_depth(entry.depth);
    packed.gen_and_type = static_cast<std::uint8_t>((generation_bits & kGenerationMask) |
                                                   (static_cast<std::uint8_t>(entry.type) & kNodeTypeMask));
    packed.packed_move = pack_move(entry.best_move);
    packed.score = entry.score;
    packed.static_eval = entry.static_eval;

    cluster.entries[slot_index].store(packed, std::memory_order_relaxed);
}

std::optional<TTEntry> GlobalTranspositionTable::probe(std::uint64_t key) const {
    std::shared_lock lock(global_mutex_);
    if (clusters_.empty()) {
        return std::nullopt;
    }

    const std::size_t index = cluster_index(key);
    const Cluster &cluster = clusters_[index];
    auto slot = find_entry(cluster, static_cast<std::uint16_t>(key));
    if (!slot.has_value()) {
        return std::nullopt;
    }

    TTEntry result;
    result.best_move = unpack_move(slot->packed_move);
    result.depth = unpack_depth(slot->depth8);
    result.score = slot->score;
    result.type = static_cast<TTNodeType>(slot->stored_type());
    result.static_eval = slot->static_eval;
    result.generation = unpack_generation(slot->gen_and_type);
    return result;
}

void GlobalTranspositionTable::prefetch(std::uint64_t key) const {
#if defined(__GNUC__) || defined(__clang__)
    std::shared_lock lock(global_mutex_, std::defer_lock);
    if (!lock.try_lock()) {
        return;
    }
    if (clusters_.empty()) {
        return;
    }
    const std::size_t index = cluster_index(key);
    const Cluster &cluster = clusters_[index];
    for (const auto &entry : cluster.entries) {
        __builtin_prefetch(static_cast<const void *>(&entry), 0, 1);
    }
#else
    (void)key;
#endif
}

bool GlobalTranspositionTable::save(const std::string &path, std::string *error) const {
    std::unique_lock lock(global_mutex_);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        if (error != nullptr) {
            *error = "No se pudo guardar la tabla de transposición: " + path;
        }
        return false;
    }

    const char magic[4] = {'S', 'R', 'T', 'T'};
    const std::uint32_t version = 2;
    const std::uint64_t bucket_count = static_cast<std::uint64_t>(clusters_.size());
    out.write(magic, sizeof(magic));
    out.write(reinterpret_cast<const char *>(&version), sizeof(version));
    out.write(reinterpret_cast<const char *>(&bucket_count), sizeof(bucket_count));
    out.write(reinterpret_cast<const char *>(&configured_size_mb_), sizeof(configured_size_mb_));
    out.write(reinterpret_cast<const char *>(&generation_counter_), sizeof(generation_counter_));
    out.write(reinterpret_cast<const char *>(&generation_tag_), sizeof(generation_tag_));

    for (const auto &cluster : clusters_) {
        for (const auto &entry_atomic : cluster.entries) {
            PackedTTEntry entry = entry_atomic.load(std::memory_order_relaxed);
            out.write(reinterpret_cast<const char *>(&entry.key16), sizeof(entry.key16));
            out.write(reinterpret_cast<const char *>(&entry.depth8), sizeof(entry.depth8));
            out.write(reinterpret_cast<const char *>(&entry.gen_and_type), sizeof(entry.gen_and_type));
            out.write(reinterpret_cast<const char *>(&entry.packed_move), sizeof(entry.packed_move));
            out.write(reinterpret_cast<const char *>(&entry.score), sizeof(entry.score));
            out.write(reinterpret_cast<const char *>(&entry.static_eval), sizeof(entry.static_eval));
        }
    }

    if (!out.good()) {
        if (error != nullptr) {
            *error = "Error al escribir la tabla de transposición";
        }
        return false;
    }
    return true;
}

bool GlobalTranspositionTable::load(const std::string &path, std::string *error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error != nullptr) {
            *error = "No se pudo abrir la tabla de transposición: " + path;
        }
        return false;
    }

    char magic[4];
    std::uint32_t version = 0;
    std::uint64_t bucket_count = 0;
    std::size_t size_mb = 0;
    std::uint8_t saved_generation_counter = 1;
    std::uint8_t saved_generation_tag = kGenerationDelta;

    in.read(magic, sizeof(magic));
    in.read(reinterpret_cast<char *>(&version), sizeof(version));
    in.read(reinterpret_cast<char *>(&bucket_count), sizeof(bucket_count));
    in.read(reinterpret_cast<char *>(&size_mb), sizeof(size_mb));
    in.read(reinterpret_cast<char *>(&saved_generation_counter), sizeof(saved_generation_counter));
    in.read(reinterpret_cast<char *>(&saved_generation_tag), sizeof(saved_generation_tag));

    if (!in.good() || std::string_view(magic, sizeof(magic)) != "SRTT" || version != 2) {
        if (error != nullptr) {
            *error = "Formato de tabla de transposición inválido";
        }
        return false;
    }

    std::vector<Cluster> new_clusters(static_cast<std::size_t>(bucket_count));
    for (auto &cluster : new_clusters) {
        for (auto &entry_atomic : cluster.entries) {
            PackedTTEntry entry;
            in.read(reinterpret_cast<char *>(&entry.key16), sizeof(entry.key16));
            in.read(reinterpret_cast<char *>(&entry.depth8), sizeof(entry.depth8));
            in.read(reinterpret_cast<char *>(&entry.gen_and_type), sizeof(entry.gen_and_type));
            in.read(reinterpret_cast<char *>(&entry.packed_move), sizeof(entry.packed_move));
            in.read(reinterpret_cast<char *>(&entry.score), sizeof(entry.score));
            in.read(reinterpret_cast<char *>(&entry.static_eval), sizeof(entry.static_eval));
            if (!in.good()) {
                if (error != nullptr) {
                    *error = "Archivo de tabla de transposición truncado";
                }
                return false;
            }
            entry_atomic.store(entry, std::memory_order_relaxed);
        }
    }

    std::unique_lock lock(global_mutex_);
    clusters_ = std::move(new_clusters);
    configured_size_mb_ = size_mb;
    generation_counter_ = saved_generation_counter == 0 ? 1 : saved_generation_counter;
    generation_tag_ = pack_generation(generation_counter_);
    if (saved_generation_tag != 0) {
        generation_tag_ = saved_generation_tag;
    }
    epoch_marker_ = transposition_table_epoch.load(std::memory_order_relaxed);
    transposition_table_size_mb.store(size_mb, std::memory_order_relaxed);
    return true;
}

void GlobalTranspositionTable::ensure_settings_locked() {
    const std::size_t desired = transposition_table_size_mb.load(std::memory_order_relaxed);
    const std::uint64_t epoch = transposition_table_epoch.load(std::memory_order_relaxed);
    if (configured_size_mb_ != desired || epoch_marker_ != epoch) {
        rebuild_locked(desired);
        epoch_marker_ = epoch;
    }
}

void GlobalTranspositionTable::rebuild_locked(std::size_t size_mb) {
    configured_size_mb_ = size_mb;
    if (size_mb == 0) {
        clusters_.clear();
        generation_counter_ = 1;
        generation_tag_ = kGenerationDelta;
        return;
    }
    std::size_t bytes = size_mb * 1024ULL * 1024ULL;
    std::size_t bucket_count = std::max<std::size_t>(1, bytes / sizeof(Cluster));
    bucket_count = std::bit_ceil(bucket_count);
    clusters_.assign(bucket_count, Cluster{});
    generation_counter_ = 1;
    generation_tag_ = kGenerationDelta;
}

std::size_t GlobalTranspositionTable::cluster_index(std::uint64_t key) const {
    const std::size_t bucket_count = clusters_.size();
    if (bucket_count == 0) {
        return 0;
    }
    return static_cast<std::size_t>(high_product(key, static_cast<std::uint64_t>(bucket_count)));
}

std::size_t GlobalTranspositionTable::select_entry_index(const Cluster &cluster, std::uint16_t key16) {
    std::size_t empty_slot = kClusterSize;
    for (std::size_t i = 0; i < cluster.entries.size(); ++i) {
        PackedTTEntry entry = cluster.entries[i].load(std::memory_order_acquire);
        if (entry.occupied() && entry.key16 == key16) {
            return i;
        }
        if (!entry.occupied() && empty_slot == kClusterSize) {
            empty_slot = i;
        }
    }
    if (empty_slot != kClusterSize) {
        return empty_slot;
    }

    std::size_t replacement = 0;
    PackedTTEntry best_entry = cluster.entries[0].load(std::memory_order_acquire);
    int best_score = replacement_score(best_entry, generation_tag_);
    for (std::size_t i = 1; i < cluster.entries.size(); ++i) {
        PackedTTEntry candidate = cluster.entries[i].load(std::memory_order_acquire);
        int candidate_score = replacement_score(candidate, generation_tag_);
        if (candidate_score < best_score) {
            replacement = i;
            best_score = candidate_score;
        }
    }
    return replacement;
}

std::optional<GlobalTranspositionTable::PackedTTEntry> GlobalTranspositionTable::find_entry(
    const Cluster &cluster, std::uint16_t key16) const {
    for (const auto &entry_atomic : cluster.entries) {
        PackedTTEntry entry = entry_atomic.load(std::memory_order_acquire);
        if (entry.occupied() && entry.key16 == key16) {
            return entry;
        }
    }
    return std::nullopt;
}

std::size_t GlobalTranspositionTable::bucket_count_for_tests() const {
    std::shared_lock lock(global_mutex_);
    return clusters_.size();
}

GlobalTranspositionTable &shared_transposition_table() {
    static GlobalTranspositionTable table;
    return table;
}

void set_transposition_table_size(std::size_t size_mb) {
    size_mb = std::clamp<std::size_t>(size_mb, std::size_t{0}, std::size_t{33'554'432});
    transposition_table_size_mb.store(size_mb, std::memory_order_relaxed);
    transposition_table_epoch.fetch_add(1, std::memory_order_relaxed);
}

std::size_t get_transposition_table_size() {
    return transposition_table_size_mb.load(std::memory_order_relaxed);
}

void clear_transposition_tables() {
    transposition_table_epoch.fetch_add(1, std::memory_order_relaxed);
}

bool save_transposition_table(const std::string &path, std::string *error) {
    return shared_transposition_table().save(path, error);
}

bool load_transposition_table(const std::string &path, std::string *error) {
    return shared_transposition_table().load(path, error);
}

}  // namespace sirio

