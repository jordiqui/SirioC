#include "pyrrhic/engine.h"
#include "uci/Uci.h"

#include <algorithm>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string decode_fen_argument(std::string fen) {
    std::replace(fen.begin(), fen.end(), '_', ' ');
    return fen;
}

}  // namespace

int main(int argc, char* argv[]) {
    for (int index = 1; index < argc; ++index) {
        if (std::string(argv[index]) == "--uci") {
            uci::loop();
            return 0;
        }
    }

    sirio::pyrrhic::Engine engine;

    bool run_cli = true;

    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "--uci") {
            continue;
        }
        if (arg == "--fen" && index + 1 < argc) {
            const std::string fen = decode_fen_argument(argv[++index]);
            try {
                engine.set_position(fen);
            } catch (const std::exception& error) {
                std::cerr << "Failed to parse FEN: " << error.what() << std::endl;
                return 1;
            }
        } else if (arg == "--pgn" && index + 1 < argc) {
            const std::string path = argv[++index];
            try {
                engine.load_game_from_file(path);
            } catch (const std::exception& error) {
                std::cerr << "Failed to load PGN: " << error.what() << std::endl;
                return 1;
            }
        } else if (arg == "--print") {
            std::cout << engine.board().pretty();
            run_cli = false;
        } else if (arg == "--evaluate") {
            std::cout << "Static evaluation: " << engine.evaluate() << std::endl;
            run_cli = false;
        } else if (arg == "--network" && index + 1 < argc) {
            const std::string path = argv[++index];
            if (!engine.load_network(path)) {
                std::cerr << "Failed to load evaluation network from " << path << std::endl;
                return 1;
            }
        } else if (arg == "--tablebase" && index + 1 < argc) {
            const std::string path = argv[++index];
            if (!engine.configure_tablebase(path)) {
                std::cerr << "Failed to initialize tablebase from " << path << std::endl;
                return 1;
            }
        } else if (arg == "--no-cli") {
            run_cli = false;
        } else if (arg == "--help") {
            std::cout << "SirioC options:\n"
                      << "  --fen <fen>     Load a FEN (use '_' for spaces)\n"
                      << "  --pgn <file>    Load moves from a PGN file\n"
                      << "  --print         Print the current board\n"
                      << "  --evaluate      Print a material evaluation\n"
                      << "  --network <f>   Load evaluation weights from file\n"
                      << "  --tablebase <f> Load CSV tablebase\n"
                      << "  --no-cli        Disable the interactive shell\n"
                      << "  --help          Show this message\n";
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            return 1;
        }
    }

    if (run_cli) {
        auto push_line_back = [](const std::string& line) {
            std::cin.putback('\n');
            for (auto it = line.rbegin(); it != line.rend(); ++it) {
                std::cin.putback(*it);
            }
        };

        std::string first_line;
        if (std::getline(std::cin, first_line)) {
            push_line_back(first_line);
            if (first_line == "uci") {
                uci::loop();
                return 0;
            }
        } else {
            std::cin.clear();
        }

        engine.run_cli(std::cin, std::cout);
    }

    return 0;
}
