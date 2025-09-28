#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace engine {

class NodeCounters {
public:
    NodeCounters() = default;

    void init(std::size_t threads) {
        locals_.reset();
        locals_size_ = threads;
        if (threads == 0) {
            published_.store(0, std::memory_order_relaxed);
            return;
        }
        locals_ = std::make_unique<std::atomic<uint64_t>[]>(threads);
        for (std::size_t i = 0; i < threads; ++i) {
            locals_[i].store(0, std::memory_order_relaxed);
        }
        published_.store(0, std::memory_order_relaxed);
    }

    void reset() {
        for (std::size_t i = 0; i < locals_size_; ++i) {
            locals_[i].store(0, std::memory_order_relaxed);
        }
        published_.store(0, std::memory_order_relaxed);
    }

    uint64_t increment(std::size_t thread_index, uint64_t delta = 1) {
        if (locals_size_ == 0) {
            return published_.fetch_add(delta, std::memory_order_relaxed) + delta;
        }
        if (thread_index >= locals_size_) {
            thread_index %= locals_size_;
        }
        auto& local = locals_[thread_index];
        uint64_t value = local.fetch_add(delta, std::memory_order_relaxed) + delta;
        if ((value & kFlushMask) == 0) {
            uint64_t expected = value;
            if (local.compare_exchange_strong(expected, 0ULL, std::memory_order_relaxed,
                                              std::memory_order_relaxed)) {
                published_.fetch_add(value, std::memory_order_relaxed);
                value = 0;
            } else {
                value = local.load(std::memory_order_relaxed);
            }
        }
        return published_.load(std::memory_order_relaxed) + value;
    }

    uint64_t publish_relaxed() {
        uint64_t total = published_.load(std::memory_order_relaxed);
        for (std::size_t i = 0; i < locals_size_; ++i) {
            uint64_t value = locals_[i].exchange(0ULL, std::memory_order_relaxed);
            if (value != 0ULL) {
                total += value;
            }
        }
        published_.store(total, std::memory_order_relaxed);
        return total;
    }

    uint64_t total_relaxed() const {
        uint64_t total = published_.load(std::memory_order_relaxed);
        for (std::size_t i = 0; i < locals_size_; ++i) {
            total += locals_[i].load(std::memory_order_relaxed);
        }
        return total;
    }

private:
    static constexpr uint64_t kFlushMask = (1ULL << 10) - 1; // flush every 1024 nodes

    std::unique_ptr<std::atomic<uint64_t>[]> locals_{};
    std::size_t locals_size_ = 0;
    std::atomic<uint64_t> published_{0};
};

inline NodeCounters& global_node_counters() {
    static NodeCounters counters;
    return counters;
}

inline void init_nodes(std::size_t threads) { global_node_counters().init(threads); }
inline void reset_nodes() { global_node_counters().reset(); }
inline uint64_t inc_node(std::size_t thread_index, uint64_t delta = 1) {
    return global_node_counters().increment(thread_index, delta);
}
inline uint64_t publish_nodes_relaxed() { return global_node_counters().publish_relaxed(); }
inline uint64_t total_nodes_relaxed() { return global_node_counters().total_relaxed(); }

} // namespace engine

