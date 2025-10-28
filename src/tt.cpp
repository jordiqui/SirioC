#include "sirio/transposition_table.hpp"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string_view>

namespace sirio {
namespace {

constexpr std::size_t kDefaultTranspositionTableSizeMb = 16;

std::atomic<std::size_t> transposition_table_size_mb{kDefaultTranspositionTableSizeMb};
std::atomic<std::uint64_t> transposition_table_epoch{1};

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
    const std::uint8_t age = static_cast<std::uint8_t>((kGenerationCycle + current_generation -
                                                        entry.stored_generation()) & kGenerationMask);
    return static_cast<int>(entry.packed_depth()) - static_cast<int>(age);
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
    std::lock_guard shard_lock(shard_mutexes_[index % kMutexShardCount]);
    Cluster &cluster = clusters_[index];
    PackedTTEntry *slot = select_entry(cluster, static_cast<std::uint16_t>(key));

    const std::uint8_t generation_bits = pack_generation(generation);
    slot->key16 = static_cast<std::uint16_t>(key);
    slot->depth8 = pack_depth(entry.depth);
    slot->gen_and_type = static_cast<std::uint8_t>((generation_bits & kGenerationMask) |
                                                   (static_cast<std::uint8_t>(entry.type) & kNodeTypeMask));
    slot->packed_move = pack_move(entry.best_move);
    slot->score = entry.score;
    slot->static_eval = entry.static_eval;
}

std::optional<TTEntry> GlobalTranspositionTable::probe(std::uint64_t key) const {
    std::shared_lock lock(global_mutex_);
    if (clusters_.empty()) {
        return std::nullopt;
    }

    const std::size_t index = cluster_index(key);
    std::lock_guard shard_lock(shard_mutexes_[index % kMutexShardCount]);
    const Cluster &cluster = clusters_[index];
    const PackedTTEntry *slot = find_entry(cluster, static_cast<std::uint16_t>(key));
    if (slot == nullptr) {
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
        for (const auto &entry : cluster.entries) {
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
        for (auto &entry : cluster.entries) {
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
    unsigned __int128 product = static_cast<unsigned __int128>(key) * bucket_count;
    return static_cast<std::size_t>(product >> 64);
}

GlobalTranspositionTable::PackedTTEntry *GlobalTranspositionTable::select_entry(Cluster &cluster,
                                                                               std::uint16_t key16) {
    PackedTTEntry *empty_slot = nullptr;
    for (auto &entry : cluster.entries) {
        if (entry.occupied() && entry.key16 == key16) {
            return &entry;
        }
        if (!entry.occupied() && empty_slot == nullptr) {
            empty_slot = &entry;
        }
    }
    if (empty_slot != nullptr) {
        return empty_slot;
    }

    PackedTTEntry *replacement = &cluster.entries[0];
    int best_score = replacement_score(*replacement, generation_tag_);
    for (int i = 1; i < kClusterSize; ++i) {
        int candidate_score = replacement_score(cluster.entries[static_cast<std::size_t>(i)], generation_tag_);
        if (candidate_score < best_score) {
            replacement = &cluster.entries[static_cast<std::size_t>(i)];
            best_score = candidate_score;
        }
    }
    return replacement;
}

const GlobalTranspositionTable::PackedTTEntry *GlobalTranspositionTable::find_entry(
    const Cluster &cluster, std::uint16_t key16) const {
    for (const auto &entry : cluster.entries) {
        if (entry.occupied() && entry.key16 == key16) {
            return &entry;
        }
    }
    return nullptr;
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
    size_mb = std::clamp<std::size_t>(size_mb, 1, 33'554'432);
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

