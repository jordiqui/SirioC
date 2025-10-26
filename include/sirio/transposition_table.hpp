#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

#include "sirio/move.hpp"

namespace sirio {

enum class TTNodeType { Exact, LowerBound, UpperBound };

struct TTEntry {
    Move best_move{};
    int depth = 0;
    int score = 0;
    TTNodeType type = TTNodeType::Exact;
    int static_eval = 0;
    std::uint8_t generation = 0;
};

class GlobalTranspositionTable {
public:
    std::uint8_t prepare_for_search();
    void store(std::uint64_t key, const TTEntry &entry, std::uint8_t generation);
    std::optional<TTEntry> probe(std::uint64_t key) const;
    bool save(const std::string &path, std::string *error) const;
    bool load(const std::string &path, std::string *error);

private:
    struct PackedTTEntry {
        std::uint64_t key = 0;
        std::int32_t score = 0;
        std::int16_t depth = -1;
        std::int32_t static_eval = 0;
        std::uint8_t type = 0;
        std::uint8_t generation = 0;
        std::uint8_t from = 0;
        std::uint8_t to = 0;
        std::uint8_t piece = 0;
        std::uint8_t captured = 0xFF;
        std::uint8_t promotion = 0xFF;
        std::uint8_t flags = 0;
    };

    static constexpr std::size_t kMutexShardCount = 64;

    void ensure_settings_locked();
    void rebuild_locked(std::size_t size_mb);
    static Move unpack_move(const PackedTTEntry &slot);
    static void pack_move(const Move &move, PackedTTEntry &slot);

    mutable std::shared_mutex global_mutex_;
    mutable std::array<std::mutex, kMutexShardCount> shard_mutexes_{};
    std::vector<PackedTTEntry> entries_;
    std::uint8_t generation_ = 1;
    std::size_t configured_size_mb_ = 0;
    std::uint64_t epoch_marker_ = 0;
};

GlobalTranspositionTable &shared_transposition_table();

void set_transposition_table_size(std::size_t size_mb);
std::size_t get_transposition_table_size();
void clear_transposition_tables();
bool save_transposition_table(const std::string &path, std::string *error = nullptr);
bool load_transposition_table(const std::string &path, std::string *error = nullptr);

}  // namespace sirio

