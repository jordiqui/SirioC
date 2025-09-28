#pragma once

#include "engine/types.hpp"

#include <atomic>
#include <cstdint>
#include <memory>

namespace engine {

class TranspositionTable {
public:
    static constexpr int kClusterSize = 4;

    void resize(std::size_t megabytes);
    void clear();
    void new_search();

    bool probe(uint64_t key, int depth, int alpha, int beta, int ply, Move& move, int& score,
               int& stored_depth, int& flag, int& eval) const;
    void store(uint64_t key, Move move, int depth, int score, int flag, int ply, int eval);
    Move probe_move(uint64_t key) const;
    int hashfull() const;

    bool empty() const { return entry_count_ == 0 || !entries_; }

private:
    struct alignas(16) Entry {
        std::atomic<uint64_t> key_eval{0};
        std::atomic<uint64_t> data{0};
    };

    std::unique_ptr<Entry[]> entries_{};
    std::size_t entry_count_ = 0;
    std::size_t cluster_mask_ = 0;
    std::atomic<uint8_t> generation_{0};

    static uint16_t pack_score(int score);
    static int unpack_score(uint16_t packed);
    static uint32_t encode_move(Move move);
    static Move decode_move(uint32_t packed);
};

} // namespace engine

