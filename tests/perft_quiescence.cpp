#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "sirio/board.hpp"
#include "sirio/search.hpp"

namespace {

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
}

}  // namespace

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

