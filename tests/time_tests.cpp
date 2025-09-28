#include "engine/util/time.hpp"
#include "engine/types.hpp"

#include <cassert>

int main() {
    using namespace engine;
    using namespace engine::time;

    Limits lim{};
    lim.wtime_ms = 300'000;
    lim.btime_ms = 300'000;
    lim.winc_ms = 2'000;
    lim.binc_ms = 2'000;
    lim.movestogo = -1;

    TimeConfig config;
    auto alloc = compute_allocation(lim, true, 10, 10, config);
    assert(alloc.moves_to_go > 0);
    assert(!alloc.panic_mode);
    assert(alloc.optimal_ms >= 7'000 && alloc.optimal_ms <= 7'600);
    assert(alloc.maximum_ms >= alloc.optimal_ms);

    // Panic scenario with very low remaining time.
    lim.wtime_ms = 30'000;
    lim.winc_ms = 0;
    alloc = compute_allocation(lim, true, 10, 70, config);
    assert(alloc.panic_mode);
    assert(alloc.maximum_ms <= 20'000);

    // Movestogo hint should be respected exactly.
    lim.wtime_ms = 120'000;
    lim.winc_ms = 1'000;
    lim.movestogo = 12;
    alloc = compute_allocation(lim, true, 0, 40, config);
    assert(alloc.moves_to_go == 12);
    assert(alloc.optimal_ms <= 12'000);

    return 0;
}
