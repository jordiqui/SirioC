#include "engine/bench/bench.hpp"

#include "engine/core/board.hpp"
#include "engine/core/perft.hpp"
#include "engine/search/search.hpp"
#include "engine/types.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace engine {

const std::vector<std::string>& bench_positions() {
    static const std::vector<std::string> positions = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/2PpP3/1p2P3/2N2N2/PPQBBPPP/R3K2R w KQkq - 0 1",
        "rnbqkb1r/pp1p1ppp/2p2n2/8/2pPP3/2P2N2/PP3PPP/RNBQKB1R w KQkq - 0 1",
        "r3k2r/pp3ppp/2n1bn2/2p5/2PP4/2N2N2/PPQ2PPP/R3K2R w KQkq - 0 1",
        "r2q1rk1/pp1bbppp/2n1pn2/2pp4/3P4/2P1PN2/PPB2PPP/RNBQR1K1 w - - 0 9",
        "r4rk1/1pp1qppp/p1np1n2/2b1p3/2B1P3/2NP1N2/PPPQ1PPP/2KR3R w - - 0 10",
        "r4rk1/pp2qppp/2n2n2/2bp4/3P4/2N1PN2/PPQ2PPP/2KR1B1R w - - 0 12",
        "r1b1kb1r/pp3ppp/2n1pn2/q1pp4/3P4/2P1PN2/PPQ2PPP/RNB1KB1R w KQkq - 2 8",
        "r1bq1rk1/ppp1bppp/2n1pn2/3p4/3P4/2N1PN2/PPQ1BPPP/R1B2RK1 w - - 4 9",
        "r1bq1rk1/ppp1bppp/2n1pn2/3p4/3P4/2N1PN2/PPQ1BPPP/R1B2RK1 b - - 5 9",
        "r3r1k1/pp1bbppp/2n2n2/2pp4/3P4/2N1PN2/PPQ1BPPP/R3R1K1 w - - 0 12",
        "r2qr1k1/pp1bbppp/2n2n2/2pp4/3P4/2N1PN2/PPQ1BPPP/R2QR1K1 w - - 2 13",
    };
    return positions;
}

namespace bench {

namespace {

struct BenchEntry {
    std::string_view fen;
};

struct PerftEntry {
    std::string_view fen;
    std::vector<std::pair<int, uint64_t>> reference;
};

const std::array<BenchEntry, 8>& bench_suite() {
    static const std::array<BenchEntry, 8> suite = {{
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"},
        {"rnbq1k1r/pp3ppp/2p2n2/3p4/3P4/2N1PN2/PP3PPP/R1BQKB1R w KQ - 0 8"},
        {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P3/2NP1N2/PPP2PPP/2KR3R w - - 0 10"},
        {"4rrk1/pp3ppp/2p2n2/3p4/3P4/2N1PN2/PP3PPP/R4RK1 w - - 0 15"},
        {"1r3rk1/5pp1/p1npbn1p/q1p1p3/2P1P3/1PN1BP2/PB3P1P/2RQ1RK1 w - - 0 17"},
        {"r2q1rk1/pp2bppp/1np1pn2/8/2PN4/2N1BP2/PP3PPP/R2Q1RK1 w - - 0 10"},
        {"r1bq1rk1/ppp1ppbp/2np1np1/8/2PP4/2N1PN2/PP2BPPP/R1BQ1RK1 w - - 0 8"},
        {"r4rk1/1pp2ppp/p1npbn2/2b1p3/2B1P3/2NP1N2/PPP2PPP/2KR3R w - - 0 12"},
    }};
    return suite;
}

const std::array<PerftEntry, 3>& perft_suite() {
    static const std::array<PerftEntry, 3> suite = {{
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
         {{1, 20ULL}, {2, 400ULL}, {3, 8902ULL}, {4, 197281ULL}, {5, 4865609ULL}}},
        {"r3k2r/p1ppqpb1/bn2pnp1/2Pp4/1p2P3/2N2N2/PPQ1BPPP/R3K2R w KQkq - 0 1",
         {{1, 48ULL}, {2, 2039ULL}, {3, 97862ULL}, {4, 4085603ULL}, {5, 193690690ULL}}},
        {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
         {{1, 14ULL}, {2, 191ULL}, {3, 2812ULL}, {4, 43238ULL}, {5, 674624ULL},
          {6, 11030083ULL}}},
    }};
    return suite;
}

} // namespace

BenchResult run(Search& search, int depth) {
    BenchResult result;
    result.depth = depth;
    const auto& suite = bench_suite();
    result.positions = static_cast<int>(suite.size());

    Board board;
    auto start = std::chrono::steady_clock::now();
    for (const auto& entry : suite) {
        if (!board.set_fen(std::string(entry.fen))) {
            continue;
        }
        Limits limits;
        limits.depth = depth;
        limits.nodes = std::max<int64_t>(20000, static_cast<int64_t>(depth) * 12500);
        auto search_result = search.find_bestmove(board, limits);
        result.nodes += search_result.nodes;
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    result.time_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
    return result;
}

PerftResult run_perft_suite(int depth) {
    PerftResult result;
    result.depth = depth;
    const auto& suite = perft_suite();
    result.positions = static_cast<int>(suite.size());

    Board board;
    auto start = std::chrono::steady_clock::now();
    bool verified = true;
    for (const auto& entry : suite) {
        if (!board.set_fen(std::string(entry.fen))) {
            verified = false;
            continue;
        }
        uint64_t nodes = perft(board, depth);
        result.nodes += nodes;

        bool matched = false;
        for (const auto& [ref_depth, ref_nodes] : entry.reference) {
            if (ref_depth == depth) {
                matched = (ref_nodes == nodes);
                break;
            }
        }
        if (!matched) {
            verified = false;
        }
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    result.time_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
    result.verified = verified;
    return result;
}

} // namespace bench

} // namespace engine

