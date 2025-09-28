#pragma once

#include <cstdint>

namespace engine {

class Search;

namespace bench {

struct BenchResult {
    uint64_t nodes = 0;
    int time_ms = 0;
    int depth = 0;
    int positions = 0;
};

struct PerftResult {
    uint64_t nodes = 0;
    int time_ms = 0;
    int depth = 0;
    int positions = 0;
    bool verified = false;
};

BenchResult run(Search& search, int depth);
PerftResult run_perft_suite(int depth);

} // namespace bench

} // namespace engine

