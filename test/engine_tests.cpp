#include "files/fen.h"
#include "files/pgn_loader.h"
#include "pyrrhic/engine.h"

#include <cstdlib>
#include <iostream>

namespace {

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Test failed: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

}  // namespace

int main() {
    using sirio::files::load_pgn;
    using sirio::files::parse_fen;
    using sirio::pyrrhic::Engine;

    const auto empty_board = parse_fen("8/8/8/8/8/8/8/8 w - - 0 1");
    expect(empty_board.generate_basic_moves().empty(), "Empty board should yield no moves");

    Engine engine;
    engine.set_position("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    expect(engine.evaluate() == 0, "Balanced kings should evaluate to 0");

    engine.set_position("4k3/8/8/8/8/8/8/4K2Q w - - 0 1");
    expect(engine.evaluate() > 0, "Extra queen should yield positive evaluation");

    const auto pgn = load_pgn("[Event \"Training\"]\n\n1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 1-0");
    expect(pgn.tags.at("Event") == "Training", "PGN tag should be parsed");
    expect(pgn.moves.size() == 6, "PGN should contain six SAN moves");
    expect(pgn.result == "1-0", "Result token should be captured");

    std::cout << "All tests passed" << std::endl;
    return EXIT_SUCCESS;
}
