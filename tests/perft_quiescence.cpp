#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>

#include "sirio/board.hpp"
#include "sirio/search.hpp"

namespace {

struct ThreadRestorer {
    explicit ThreadRestorer(int previous) : previous_threads(previous) {}
    ~ThreadRestorer() { sirio::set_search_threads(previous_threads); }

    ThreadRestorer(const ThreadRestorer &) = delete;
    ThreadRestorer &operator=(const ThreadRestorer &) = delete;

    int previous_threads;
};

void benchmark_start_position_quiescence() {
    sirio::Board board;
    sirio::SearchLimits limits;
    limits.max_depth = 4;
    limits.move_time = 150;

    int previous_threads = sirio::get_search_threads();
    sirio::set_search_threads(1);
    ThreadRestorer restore(previous_threads);

    auto start = std::chrono::steady_clock::now();
    sirio::SearchResult result = sirio::search_best_move(board, limits);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    assert(result.nodes > 0);

    std::uint64_t elapsed_ms = static_cast<std::uint64_t>(elapsed.count());
    elapsed_ms = std::max<std::uint64_t>(elapsed_ms, 1);

    std::uint64_t computed_nps = (result.nodes * 1000ULL) / elapsed_ms;
    constexpr std::uint64_t minimum_nps = 1000ULL;

    assert(result.nodes_per_second >= minimum_nps || computed_nps >= minimum_nps);
}

}  // namespace

void run_quiescence_benchmarks() { benchmark_start_position_quiescence(); }

