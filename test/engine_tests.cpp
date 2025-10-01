#include "files/fen.h"
#include "files/pgn_loader.h"
#include "pyrrhic/engine.h"
#include "uci/Options.h"
#include "uci/Uci.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
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

    namespace fs = std::filesystem;
    const auto original_engine_dir = g_engine_dir;
    const fs::path temp_root = fs::temp_directory_path() / "sirio_nnue_option_test";
    const fs::path resources_dir = temp_root / "resources";
    const fs::path nnue_file = resources_dir / "sirio_default.nnue";
    fs::create_directories(resources_dir);
    {
        std::ofstream out(nnue_file);
        out << "nnue";
    }

    g_engine_dir = temp_root;
    init_options();
    expect(OptionsMap.contains("EvalFile"), "EvalFile option should be available");

    OptionsMap.set("EvalFile", "<empty>");
    expect(OptionsMap.at("EvalFile").s.empty(), "EvalFile <empty> should clear the string");

    OptionsMap.set("EvalFile", "<sirio_default.nnue>");
    expect(OptionsMap.at("EvalFile").s == "sirio_default.nnue",
           "EvalFile should strip angle brackets");

    const auto resolved = uci::resolve_nnue_path_for_tests(OptionsMap.at("EvalFile").s);
    expect(resolved == nnue_file,
           "EvalFile should resolve to the NNUE file in the engine directory");

    OptionsMap.set("EvalFile", "<empty>");
    expect(OptionsMap.at("EvalFile").s.empty(),
           "EvalFile should clear again when set to <empty>");

    g_engine_dir = original_engine_dir;
    fs::remove(nnue_file);
    fs::remove(resources_dir);
    fs::remove(temp_root);

    std::cout << "All tests passed" << std::endl;
    return EXIT_SUCCESS;
}
