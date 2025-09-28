#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace engine {

// Lightweight per-thread node counter with relaxed aggregation.
class NodesCounter {
public:
    NodesCounter() = default;

    void configure(size_t threads) {
        if (locals_.size() != threads) {
            locals_.assign(threads, 0);
        } else {
            std::fill(locals_.begin(), locals_.end(), 0);
        }
        total_.store(0, std::memory_order_relaxed);
    }

    uint64_t increment(size_t thread_index, uint64_t delta = 1) {
        if (thread_index >= locals_.size()) {
            return total_.fetch_add(delta, std::memory_order_relaxed) + delta;
        }
        locals_[thread_index] += delta;
        return total_.fetch_add(delta, std::memory_order_relaxed) + delta;
    }

    uint64_t total_nodes() const { return total_.load(std::memory_order_relaxed); }

private:
    std::vector<uint64_t> locals_{};
    std::atomic<uint64_t> total_{0};
};

} // namespace engine

