#include "engine/core/board.hpp"
#include "engine/core/perft.hpp"

#include <array>
#include <cassert>

int main() {
    using namespace engine;

    Board board;
    board.set_startpos();

    constexpr std::array<uint64_t, 3> kExpected = {20ULL, 400ULL, 8902ULL};
    for (std::size_t depth = 1; depth <= kExpected.size(); ++depth) {
        uint64_t nodes = perft(board, static_cast<int>(depth));
        assert(nodes == kExpected[depth - 1]);
        assert(board.white_to_move());
    }

    return 0;
}
