codex/modificar-generate_tactical_moves-para-jaques-y-promociones
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
=======
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
 main

#include "sirio/board.hpp"
#include "sirio/search.hpp"

namespace {

 codex/modificar-generate_tactical_moves-para-jaques-y-promociones
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
=======
struct QuiescenceBenchmark {
    std::string fen;
    int depth = 0;
    std::uint64_t max_nodes = 0;
};

std::uint64_t run_quiescence_search(const sirio::Board &board, int depth) {
    sirio::SearchLimits limits;
    limits.max_depth = depth;
    limits.infinite = false;
    auto result = sirio::search_best_move(board, limits);
    return result.nodes;
}

void benchmark_position(const QuiescenceBenchmark &benchmark) {
    sirio::Board board;
    board.set_from_fen(benchmark.fen);
    std::uint64_t nodes = run_quiescence_search(board, benchmark.depth);
    std::cout << "quiescence benchmark fen='" << benchmark.fen << "' depth="
              << benchmark.depth << " nodes=" << nodes << '\n';
    assert(nodes <= benchmark.max_nodes);
 main
}

}  // namespace

 codex/modificar-generate_tactical_moves-para-jaques-y-promociones
void run_quiescence_benchmarks() { benchmark_start_position_quiescence(); }
=======
void run_quiescence_perft_benchmarks() {
    const int previous_threads = sirio::get_search_threads();
    sirio::set_search_threads(1);
    const std::vector<QuiescenceBenchmark> benchmarks = {
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 1, 2'000},
        {"rnbq1rk1/ppp2ppp/4pn2/3p4/3P4/2NBPN2/PP3PPP/R1BQ1RK1 w - - 0 7", 1, 5'000},
        {"2r2rk1/pp1n1ppp/2p1pn2/3p4/3P4/1PN1PN2/PB3PPP/2RR2K1 w - - 0 1", 2, 50'000},
    };

    for (const auto &benchmark : benchmarks) {
        benchmark_position(benchmark);
    }
    sirio::set_search_threads(previous_threads);
}
 main

