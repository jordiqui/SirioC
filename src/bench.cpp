#include "engine/core/board.hpp"

#include <string>
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

} // namespace engine

