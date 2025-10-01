#include "pyrrhic/engine.h"
#include "uci/Uci.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <system_error>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

std::string decode_fen_argument(std::string fen) {
    std::replace(fen.begin(), fen.end(), '_', ' ');
    return fen;
}

std::filesystem::path determine_engine_directory(const char* argv0) {
#ifdef _WIN32
    std::wstring buffer(MAX_PATH ? MAX_PATH : 1, L'\0');
    for (;;) {
        DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                          static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                buffer.resize(buffer.size() * 2, L'\0');
                continue;
            }
            break;
        }

        if (length < buffer.size()) {
            if (buffer.size() <= static_cast<size_t>(length)) {
                break;
            }
            buffer[length] = L'\0';
            std::wstring module_path(buffer.c_str(), length);
            return std::filesystem::path(module_path).parent_path();
        }

        if (buffer.size() == 0) {
            break;
        }

        size_t next_size = buffer.size() * 2;
        if (next_size <= buffer.size()) {
            break;
        }
        buffer.resize(next_size, L'\0');
    }
#else
    std::error_code ec_self;
    auto self_path = std::filesystem::canonical("/proc/self/exe", ec_self);
    if (!ec_self) {
        return self_path.parent_path();
    }
#endif

    std::error_code ec;
    if (argv0) {
        auto canonical = std::filesystem::canonical(argv0, ec);
        if (!ec) {
            return canonical.parent_path();
        }
        canonical = std::filesystem::absolute(argv0, ec);
        if (!ec) {
            return canonical.parent_path();
        }
    }

    auto current = std::filesystem::current_path(ec);
    if (!ec) {
        return current;
    }
    return {};
}

}  // namespace

int main(int argc, char* argv[]) {
    g_engine_dir = determine_engine_directory(argc > 0 ? argv[0] : nullptr);
    init_options();

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
