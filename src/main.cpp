#include <cctype>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "sirio/board.hpp"
#include "sirio/move.hpp"
#include "sirio/movegen.hpp"
#include "sirio/evaluation.hpp"
#include "sirio/search.hpp"
#include "sirio/syzygy.hpp"

namespace {

struct EngineOptions {
    bool use_nnue = false;
    std::string nnue_file;
};

struct BackendState {
    bool nnue_active = false;
    std::string nnue_path;
};

EngineOptions engine_options;
BackendState backend_state;

void apply_evaluation_options(const EngineOptions &options, const sirio::Board &board) {
    if (!options.use_nnue) {
        if (backend_state.nnue_active) {
            use_classical_evaluation();
            backend_state.nnue_active = false;
            backend_state.nnue_path.clear();
        } else {
            use_classical_evaluation();
        }
        initialize_evaluation(board);
        return;
    }

    if (options.nnue_file.empty()) {
        std::cout << "info string NNUE is enabled but NNUEFile is empty" << std::endl;
        use_classical_evaluation();
        backend_state.nnue_active = false;
        backend_state.nnue_path.clear();
        initialize_evaluation(board);
        return;
    }

    if (backend_state.nnue_active && backend_state.nnue_path == options.nnue_file) {
        initialize_evaluation(board);
        return;
    }

    std::string error;
    if (auto backend = make_nnue_evaluation(options.nnue_file, &error)) {
        set_evaluation_backend(std::move(backend));
        backend_state.nnue_active = true;
        backend_state.nnue_path = options.nnue_file;
        initialize_evaluation(board);
    } else {
        std::cout << "info string Failed to load NNUE: " << error << std::endl;
        use_classical_evaluation();
        backend_state.nnue_active = false;
        backend_state.nnue_path.clear();
        initialize_evaluation(board);
    }
}

void send_uci_id() {
    std::cout << "id name SirioC" << std::endl;
    std::cout << "id author OpenAI" << std::endl;
    std::cout << "option name SyzygyPath type string default" << std::endl;
    std::cout << "option name UseNNUE type check default false" << std::endl;
    std::cout << "option name NNUEFile type string default" << std::endl;
    std::cout << "uciok" << std::endl;
}

void send_ready() { std::cout << "readyok" << std::endl; }

std::string trim_leading(std::string_view view) {
    std::size_t pos = 0;
    while (pos < view.size() && std::isspace(static_cast<unsigned char>(view[pos]))) {
        ++pos;
    }
    return std::string{view.substr(pos)};
}

void set_position(sirio::Board &board, const std::string &command_args) {
    std::istringstream stream{command_args};
    std::string token;
    if (!(stream >> token)) {
        return;
    }

    if (token == "startpos") {
        board = sirio::Board{};
        if (!(stream >> token)) {
            return;
        }
    } else if (token == "fen") {
        std::string fen_parts[6];
        for (int i = 0; i < 6; ++i) {
            if (!(stream >> fen_parts[i])) {
                throw std::runtime_error("Invalid FEN in position command");
            }
        }
        board = sirio::Board{fen_parts[0] + " " + fen_parts[1] + " " + fen_parts[2] + " " +
                             fen_parts[3] + " " + fen_parts[4] + " " + fen_parts[5]};
        if (!(stream >> token)) {
            return;
        }
    } else {
        throw std::runtime_error("Unsupported position command");
    }

    if (token != "moves") {
        // Skip until we find moves keyword or end
        while (stream >> token) {
            if (token == "moves") {
                break;
            }
        }
    }

    if (token != "moves") {
        return;
    }

    while (stream >> token) {
        try {
            sirio::Move move = sirio::move_from_uci(board, token);
            board = board.apply_move(move);
        } catch (const std::exception &) {
            // Ignore illegal moves in history to maintain robustness
            break;
        }
    }

    initialize_evaluation(board);
}

void handle_setoption(const std::string &args, sirio::Board &board) {
    std::istringstream stream{args};
    std::string token;
    std::string name;
    std::string value;
    while (stream >> token) {
        if (token == "name") {
            std::string word;
            while (stream >> word && word != "value") {
                if (!name.empty()) {
                    name += ' ';
                }
                name += word;
            }
            if (word != "value") {
                return;
            }
            std::string remainder;
            std::getline(stream, remainder);
            value = trim_leading(remainder);
            break;
        }
    }

    if (name == "SyzygyPath") {
        sirio::syzygy::set_tablebase_path(value);
    } else if (name == "UseNNUE") {
        std::string lowered = value;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (lowered == "true" || lowered == "1" || lowered == "on" || lowered == "yes") {
            engine_options.use_nnue = true;
        } else if (lowered == "false" || lowered == "0" || lowered == "off" || lowered == "no") {
            engine_options.use_nnue = false;
        }
        apply_evaluation_options(engine_options, board);
    } else if (name == "NNUEFile") {
        engine_options.nnue_file = value;
        apply_evaluation_options(engine_options, board);
    }
}

void handle_go(const std::string &command_args, const sirio::Board &board) {
    std::istringstream stream{command_args};
    std::string token;
    sirio::SearchLimits limits;
    bool depth_overridden = false;
    bool has_time_information = false;
    while (stream >> token) {
        if (token == "depth") {
            if (stream >> token) {
                limits.max_depth = std::stoi(token);
                depth_overridden = true;
            }
        } else if (token == "nodes") {
            if (stream >> token) {
                long long parsed = std::stoll(token);
                if (parsed > 0) {
                    limits.max_nodes = static_cast<std::uint64_t>(parsed);
                } else {
                    limits.max_nodes = 0;
                }
            }
        } else if (token == "movetime") {
            if (stream >> token) {
                limits.move_time = std::stoi(token);
                has_time_information = true;
                limits.max_depth = 64;
            }
        } else if (token == "wtime") {
            if (stream >> token) {
                limits.time_left_white = std::stoi(token);
                has_time_information = true;
            }
        } else if (token == "btime") {
            if (stream >> token) {
                limits.time_left_black = std::stoi(token);
                has_time_information = true;
            }
        } else if (token == "winc") {
            if (stream >> token) {
                limits.increment_white = std::stoi(token);
                has_time_information = true;
            }
        } else if (token == "binc") {
            if (stream >> token) {
                limits.increment_black = std::stoi(token);
                has_time_information = true;
            }
        } else if (token == "movestogo") {
            if (stream >> token) {
                limits.moves_to_go = std::stoi(token);
                has_time_information = true;
            }
        } else if (token == "infinite") {
            limits.max_depth = 64;
        }
    }

    if (has_time_information && !depth_overridden && limits.move_time == 0) {
        limits.max_depth = 64;
    }

    apply_evaluation_options(engine_options, board);
    sirio::SearchResult result = sirio::search_best_move(board, limits);
    if (result.has_move) {
        int reported_depth = result.depth_reached > 0 ? result.depth_reached : limits.max_depth;
        std::cout << "info depth " << reported_depth << " score cp " << result.score << " pv "
                  << sirio::move_to_uci(result.best_move) << std::endl;
        std::cout << "bestmove " << sirio::move_to_uci(result.best_move) << std::endl;
    } else {
        auto legal_moves = sirio::generate_legal_moves(board);
        if (!legal_moves.empty()) {
            const auto fallback = legal_moves.front();
            std::cout << "bestmove " << sirio::move_to_uci(fallback) << std::endl;
        } else {
            std::cout << "bestmove 0000" << std::endl;
        }
    }
}

}  // namespace

int main() {
    sirio::Board board;
    std::string line;

    while (std::getline(std::cin, line)) {
        std::string trimmed = trim_leading(line);
        if (trimmed.empty()) {
            continue;
        }

        std::istringstream stream{trimmed};
        std::string command;
        stream >> command;

        try {
            if (command == "uci") {
                send_uci_id();
            } else if (command == "isready") {
                send_ready();
            } else if (command == "ucinewgame") {
                board = sirio::Board{};
                apply_evaluation_options(engine_options, board);
            } else if (command == "position") {
                std::string rest;
                std::getline(stream, rest);
                set_position(board, rest);
            } else if (command == "go") {
                std::string rest;
                std::getline(stream, rest);
                handle_go(rest, board);
            } else if (command == "setoption") {
                std::string rest;
                std::getline(stream, rest);
                handle_setoption(rest, board);
            } else if (command == "quit" || command == "stop") {
                break;
            } else if (command == "d") {
                std::cout << board.to_fen() << std::endl;
            }
        } catch (const std::exception &ex) {
            std::cerr << "Error: " << ex.what() << std::endl;
        }
    }

    return 0;
}

