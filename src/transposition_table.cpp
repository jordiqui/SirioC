#include "sirio/transposition_table.hpp"

#include <algorithm>
#include <atomic>
#include <bit>
#include <fstream>
#include <string_view>

namespace sirio {
namespace {

constexpr std::size_t kDefaultTranspositionTableSizeMb = 16;

std::atomic<std::size_t> transposition_table_size_mb{kDefaultTranspositionTableSizeMb};
std::atomic<std::uint64_t> transposition_table_epoch{1};

}  // namespace

std::uint8_t GlobalTranspositionTable::prepare_for_search() {
    std::unique_lock lock(global_mutex_);
    ensure_settings_locked();
    ++generation_;
    if (generation_ == 0) {
        generation_ = 1;
    }
    return generation_;
}

void GlobalTranspositionTable::store(std::uint64_t key, const TTEntry &entry,
                                     std::uint8_t generation) {
    std::shared_lock lock(global_mutex_);
    if (entries_.empty()) {
        return;
    }
    const std::size_t index = static_cast<std::size_t>(key & (entries_.size() - 1));
    std::lock_guard shard_lock(shard_mutexes_[index % kMutexShardCount]);
    PackedTTEntry &slot = entries_[index];
    if (slot.depth < entry.depth || slot.key != key || slot.generation != generation ||
        entry.type == TTNodeType::Exact || slot.depth < 0) {
        slot.key = key;
        pack_move(entry.best_move, slot);
        slot.score = entry.score;
        slot.depth = static_cast<std::int16_t>(entry.depth);
        slot.static_eval = entry.static_eval;
        slot.type = static_cast<std::uint8_t>(entry.type);
        slot.generation = generation;
    }
}

std::optional<TTEntry> GlobalTranspositionTable::probe(std::uint64_t key) const {
    std::shared_lock lock(global_mutex_);
    if (entries_.empty()) {
        return std::nullopt;
    }
    const std::size_t index = static_cast<std::size_t>(key & (entries_.size() - 1));
    std::lock_guard shard_lock(shard_mutexes_[index % kMutexShardCount]);
    const PackedTTEntry &slot = entries_[index];
    if (slot.depth < 0 || slot.key != key) {
        return std::nullopt;
    }
    TTEntry entry;
    entry.best_move = unpack_move(slot);
    entry.score = slot.score;
    entry.depth = slot.depth;
    entry.type = static_cast<TTNodeType>(slot.type);
    entry.static_eval = slot.static_eval;
    entry.generation = slot.generation;
    return entry;
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
    const std::uint32_t version = 1;
    const std::uint64_t count = static_cast<std::uint64_t>(entries_.size());
    out.write(magic, sizeof(magic));
    out.write(reinterpret_cast<const char *>(&version), sizeof(version));
    out.write(reinterpret_cast<const char *>(&count), sizeof(count));
    out.write(reinterpret_cast<const char *>(&configured_size_mb_), sizeof(configured_size_mb_));
    out.write(reinterpret_cast<const char *>(&generation_), sizeof(generation_));
    for (const auto &slot : entries_) {
        out.write(reinterpret_cast<const char *>(&slot.key), sizeof(slot.key));
        out.write(reinterpret_cast<const char *>(&slot.score), sizeof(slot.score));
        out.write(reinterpret_cast<const char *>(&slot.depth), sizeof(slot.depth));
        out.write(reinterpret_cast<const char *>(&slot.static_eval), sizeof(slot.static_eval));
        out.write(reinterpret_cast<const char *>(&slot.type), sizeof(slot.type));
        out.write(reinterpret_cast<const char *>(&slot.generation), sizeof(slot.generation));
        out.write(reinterpret_cast<const char *>(&slot.from), sizeof(slot.from));
        out.write(reinterpret_cast<const char *>(&slot.to), sizeof(slot.to));
        out.write(reinterpret_cast<const char *>(&slot.piece), sizeof(slot.piece));
        out.write(reinterpret_cast<const char *>(&slot.captured), sizeof(slot.captured));
        out.write(reinterpret_cast<const char *>(&slot.promotion), sizeof(slot.promotion));
        out.write(reinterpret_cast<const char *>(&slot.flags), sizeof(slot.flags));
    }
    if (!out.good() && error != nullptr) {
        *error = "Error al escribir la tabla de transposición";
    }
    return out.good();
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
    std::uint64_t count = 0;
    std::size_t size_mb = 0;
    std::uint8_t saved_generation = 1;
    in.read(magic, sizeof(magic));
    in.read(reinterpret_cast<char *>(&version), sizeof(version));
    in.read(reinterpret_cast<char *>(&count), sizeof(count));
    in.read(reinterpret_cast<char *>(&size_mb), sizeof(size_mb));
    in.read(reinterpret_cast<char *>(&saved_generation), sizeof(saved_generation));
    if (!in.good() || std::string_view(magic, sizeof(magic)) != "SRTT" || version != 1) {
        if (error != nullptr) {
            *error = "Formato de tabla de transposición inválido";
        }
        return false;
    }
    std::vector<PackedTTEntry> new_entries(static_cast<std::size_t>(count));
    for (std::size_t i = 0; i < new_entries.size(); ++i) {
        PackedTTEntry slot;
        in.read(reinterpret_cast<char *>(&slot.key), sizeof(slot.key));
        in.read(reinterpret_cast<char *>(&slot.score), sizeof(slot.score));
        in.read(reinterpret_cast<char *>(&slot.depth), sizeof(slot.depth));
        in.read(reinterpret_cast<char *>(&slot.static_eval), sizeof(slot.static_eval));
        in.read(reinterpret_cast<char *>(&slot.type), sizeof(slot.type));
        in.read(reinterpret_cast<char *>(&slot.generation), sizeof(slot.generation));
        in.read(reinterpret_cast<char *>(&slot.from), sizeof(slot.from));
        in.read(reinterpret_cast<char *>(&slot.to), sizeof(slot.to));
        in.read(reinterpret_cast<char *>(&slot.piece), sizeof(slot.piece));
        in.read(reinterpret_cast<char *>(&slot.captured), sizeof(slot.captured));
        in.read(reinterpret_cast<char *>(&slot.promotion), sizeof(slot.promotion));
        in.read(reinterpret_cast<char *>(&slot.flags), sizeof(slot.flags));
        if (!in.good()) {
            if (error != nullptr) {
                *error = "Archivo de tabla de transposición truncado";
            }
            return false;
        }
        new_entries[i] = slot;
    }

    std::unique_lock lock(global_mutex_);
    entries_ = std::move(new_entries);
    configured_size_mb_ = size_mb;
    generation_ = saved_generation == 0 ? 1 : saved_generation;
    epoch_marker_ = transposition_table_epoch.load(std::memory_order_relaxed);
    transposition_table_size_mb.store(size_mb, std::memory_order_relaxed);
    return true;
}

void GlobalTranspositionTable::ensure_settings_locked() {
    const std::size_t desired =
        transposition_table_size_mb.load(std::memory_order_relaxed);
    const std::uint64_t epoch =
        transposition_table_epoch.load(std::memory_order_relaxed);
    if (configured_size_mb_ != desired || epoch_marker_ != epoch) {
        rebuild_locked(desired);
        epoch_marker_ = epoch;
    }
}

void GlobalTranspositionTable::rebuild_locked(std::size_t size_mb) {
    configured_size_mb_ = size_mb;
    if (size_mb == 0) {
        entries_.clear();
        generation_ = 1;
        return;
    }
    std::size_t bytes = size_mb * 1024ULL * 1024ULL;
    std::size_t count = std::max<std::size_t>(1, bytes / sizeof(PackedTTEntry));
    std::size_t pow2 = std::bit_ceil(count);
    entries_.assign(pow2, PackedTTEntry{});
    generation_ = 1;
}

Move GlobalTranspositionTable::unpack_move(const PackedTTEntry &slot) {
    Move move{};
    move.from = slot.from;
    move.to = slot.to;
    move.piece = static_cast<PieceType>(slot.piece);
    if (slot.captured != 0xFF) {
        move.captured = static_cast<PieceType>(slot.captured);
    }
    if (slot.promotion != 0xFF) {
        move.promotion = static_cast<PieceType>(slot.promotion);
    }
    move.is_en_passant = (slot.flags & 0x1) != 0;
    move.is_castling = (slot.flags & 0x2) != 0;
    return move;
}

void GlobalTranspositionTable::pack_move(const Move &move, PackedTTEntry &slot) {
    slot.from = static_cast<std::uint8_t>(move.from);
    slot.to = static_cast<std::uint8_t>(move.to);
    slot.piece = static_cast<std::uint8_t>(move.piece);
    slot.captured = move.captured.has_value() ? static_cast<std::uint8_t>(*move.captured) : 0xFF;
    slot.promotion = move.promotion.has_value() ? static_cast<std::uint8_t>(*move.promotion) : 0xFF;
    slot.flags = (move.is_en_passant ? 0x1 : 0x0) | (move.is_castling ? 0x2 : 0x0);
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

