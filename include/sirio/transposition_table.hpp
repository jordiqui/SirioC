#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <string>
#include <type_traits>
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
    void prefetch(std::uint64_t key) const;
    bool save(const std::string &path, std::string *error) const;
    bool load(const std::string &path, std::string *error);

    static constexpr std::size_t cluster_capacity() { return kClusterSize; }
    std::size_t bucket_count_for_tests() const;

private:
    static constexpr int kClusterSize = 3;
    static constexpr int kDepthOffset = 64;
    static constexpr std::uint8_t kNodeTypeMask = 0x3;
    static constexpr std::uint8_t kGenerationDelta = 1u << 2;
    static constexpr std::uint8_t kGenerationMask = static_cast<std::uint8_t>(0xFF & ~kNodeTypeMask);
    static constexpr int kGenerationCycle = 255 + kGenerationDelta;

    struct PackedTTEntry {
        std::uint16_t key16 = 0;
        std::uint8_t depth8 = 0;
        std::uint8_t gen_and_type = 0;
        std::uint32_t packed_move = 0;
        std::int32_t score = 0;
        std::int32_t static_eval = 0;

        bool occupied() const { return depth8 != 0; }
        std::uint8_t stored_generation() const { return gen_and_type & kGenerationMask; }
        std::uint8_t stored_type() const { return gen_and_type & kNodeTypeMask; }
        int packed_depth() const { return depth8; }
    };

    static_assert(std::is_trivially_copyable_v<PackedTTEntry>,
                  "PackedTTEntry must be trivially copyable");
    static_assert(sizeof(PackedTTEntry) <= 16, "PackedTTEntry must remain compact");

    struct alignas(64) Cluster {
        std::array<std::atomic<PackedTTEntry>, kClusterSize> entries;

        Cluster() {
            for (auto &entry : entries) {
                entry.store(PackedTTEntry{}, std::memory_order_relaxed);
            }
        }

        Cluster(const Cluster &other) {
            for (std::size_t i = 0; i < entries.size(); ++i) {
                entries[i].store(other.entries[i].load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
            }
        }

        Cluster &operator=(const Cluster &other) {
            if (this != &other) {
                for (std::size_t i = 0; i < entries.size(); ++i) {
                    entries[i].store(other.entries[i].load(std::memory_order_relaxed),
                                     std::memory_order_relaxed);
                }
            }
            return *this;
        }

        Cluster(Cluster &&other) noexcept {
            for (std::size_t i = 0; i < entries.size(); ++i) {
                entries[i].store(other.entries[i].load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
            }
        }

        Cluster &operator=(Cluster &&other) noexcept {
            if (this != &other) {
                for (std::size_t i = 0; i < entries.size(); ++i) {
                    entries[i].store(other.entries[i].load(std::memory_order_relaxed),
                                     std::memory_order_relaxed);
                }
            }
            return *this;
        }
    };

    static_assert(alignof(Cluster) >= 64, "Cluster must avoid false sharing");

    void ensure_settings_locked();
    void rebuild_locked(std::size_t size_mb);
    std::size_t cluster_index(std::uint64_t key) const;
    std::size_t select_entry_index(const Cluster &cluster, std::uint16_t key16);
    std::optional<PackedTTEntry> find_entry(const Cluster &cluster, std::uint16_t key16) const;

    static std::uint8_t pack_generation(std::uint8_t generation);
    static std::uint8_t unpack_generation(std::uint8_t gen_and_type);
    static std::uint8_t pack_depth(int depth);
    static int unpack_depth(std::uint8_t depth8);
    static std::uint32_t pack_move(const Move &move);
    static Move unpack_move(std::uint32_t packed_move);
    static int replacement_score(const PackedTTEntry &entry, std::uint8_t current_generation);

    mutable std::shared_mutex global_mutex_;
    std::vector<Cluster> clusters_;
    std::uint8_t generation_counter_ = 1;
    std::uint8_t generation_tag_ = kGenerationDelta;
    std::size_t configured_size_mb_ = 0;
    std::uint64_t epoch_marker_ = 0;
};

GlobalTranspositionTable &shared_transposition_table();

void set_transposition_table_size(std::size_t size_mb);
std::size_t get_transposition_table_size();
void clear_transposition_tables();
bool save_transposition_table(const std::string &path, std::string *error = nullptr);
bool load_transposition_table(const std::string &path, std::string *error = nullptr);

bool transposition_table_large_pages_supported();
bool transposition_table_large_pages_enabled();

}  // namespace sirio

