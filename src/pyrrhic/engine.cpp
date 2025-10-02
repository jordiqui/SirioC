#include "pyrrhic/engine.h"

#include "files/fen.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <limits>
#include <sstream>

namespace sirio::pyrrhic {

namespace {

std::string trim(std::string value) {
    const auto is_space = [](unsigned char ch) { return std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char ch) {
                  return !is_space(ch);
              }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char ch) {
                  return !is_space(ch);
              }).base(),
                value.end());
    return value;
}

constexpr const char* kEngineIdName = "SirioC-0.1.0";
constexpr const char* kEngineIdAuthor = "Jorge Ruiz credits Codex open IA";

}  // namespace

Engine::Engine()
    : tablebase_ready_(false) {
    load_network("resources/network.dat");
    configure_tablebase(SIRIO_TB_DEFAULT_PATH);
    reset();
}

Engine::~Engine() {
    sirio_tb_shutdown();
}

void Engine::reset() {
    board_ = sirio::files::parse_fen("rn1qkbnr/pppb1ppp/4p3/3p4/3P4/4PN2/PPP2PPP/RNBQKB1R w KQkq - 0 1");
    game_ = {};
}

void Engine::set_position(const std::string& fen) {
    board_ = sirio::files::parse_fen(fen);
}

std::string Engine::current_fen() const {
    return sirio::files::to_fen(board_);
}

int Engine::evaluate() const {
    return evaluator_.evaluate(board_);
}

std::vector<Move> Engine::generate_moves() const {
    return board_.generate_basic_moves();
}

std::optional<Move> Engine::suggest_move() const {
    const auto moves = generate_moves();
    if (moves.empty()) {
        return std::nullopt;
    }

    const auto& squares = board_.squares();
    std::optional<Move> best_move;
    int best_score = std::numeric_limits<int>::min();

    for (const auto& move : moves) {
        int score = 0;
        if (move.capture.has_value()) {
            score += evaluator_.piece_value(move.capture->type);
        }

        const auto& piece = squares.at(static_cast<std::size_t>(move.from));
        if (piece.has_value()) {
            score -= evaluator_.piece_value(piece->type) / 10;
        }

        if (score > best_score) {
            best_score = score;
            best_move = move;
        }
    }

    return best_move;
}

std::optional<TablebaseResult> Engine::probe_tablebase() const {
    if (!tablebase_ready_) {
        return std::nullopt;
    }

    const auto fen = current_fen();
    sirio_tb_result raw{};
    if (!sirio_tb_probe_fen(fen.c_str(), &raw)) {
        return std::nullopt;
    }

    return TablebaseResult{raw.wdl, raw.dtm};
}

void Engine::load_game(const sirio::files::PgnGame& game) {
    game_ = game;
}

void Engine::load_game_from_file(const std::string& path) {
    load_game(sirio::files::load_pgn_from_file(path));
}

bool Engine::load_network(const std::string& path) {
    return evaluator_.load_network(path);
}

bool Engine::configure_tablebase(const std::string& path) {
    tablebase_source_ = path;
    tablebase_ready_ = sirio_tb_init(path.c_str()) != 0;
    return tablebase_ready_;
}

void Engine::run_cli(std::istream& input, std::ostream& output) {
    std::string line;

    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        std::istringstream command(line);
        std::string token;
        command >> token;

        if (token == "quit" || token == "exit") {
            output << "Bye!" << std::endl;
            break;
        }

        if (token == "help") {
            output << "Commands:\n"
                   << "  help          Show this message\n"
                   << "  uci           Print the UCI identification banner\n"
                   << "  isready       Report readiness for UCI controllers\n"
                   << "  ucinewgame    Reset to the initial position\n"
                   << "  fen <string>  Load a FEN position\n"
                   << "  show          Print the current board\n"
                   << "  eval          Evaluate the current position\n"
                   << "  moves         List pseudo-legal moves\n"
                   << "  best          Suggest a material-based move\n"
                   << "  tb            Probe the endgame tablebase\n"
                   << "  load <file>   Load a PGN game\n"
                   << "  game          Show loaded PGN metadata\n"
                   << "  quit          Exit the shell" << std::endl;
            continue;
        }

        if (token == "uci") {
            output << "id name " << kEngineIdName << '\n'
                   << "id author " << kEngineIdAuthor << '\n'
                   << "uciok" << std::endl;
            continue;
        }

        if (token == "isready") {
            output << "readyok" << std::endl;
            continue;
        }

        if (token == "ucinewgame") {
            reset();
            output << "New game initialized." << std::endl;
            continue;
        }

        if (token == "fen") {
            std::string fen;
            std::getline(command, fen);
            fen = trim(fen);
#if defined(__cpp_exceptions)
            try {
                set_position(fen);
                output << "Position updated." << std::endl;
            } catch (const std::exception& error) {
                output << "Failed to parse FEN: " << error.what() << std::endl;
            }
#else
            std::string error;
            if (!sirio::files::try_parse_fen(fen, &error)) {
                output << "Failed to parse FEN: " << error << std::endl;
            } else {
                set_position(fen);
                output << "Position updated." << std::endl;
            }
#endif
            continue;
        }

        if (token == "show") {
            output << board_.pretty();
            continue;
        }

        if (token == "eval") {
            output << "Static evaluation: " << evaluate() << std::endl;
            continue;
        }

        if (token == "moves") {
            const auto moves = generate_moves();
            if (moves.empty()) {
                output << "No moves available." << std::endl;
            } else {
                output << "Pseudo-legal moves (" << moves.size() << "): ";
                for (std::size_t index = 0; index < moves.size(); ++index) {
                    if (index != 0) {
                        output << ", ";
                    }
                    output << moves[index].to_string();
                }
                output << std::endl;
            }
            continue;
        }

        if (token == "best") {
            const auto best_move = suggest_move();
            if (best_move.has_value()) {
                output << "Best capture heuristic: " << best_move->to_string() << std::endl;
            } else {
                output << "No move suggestion available." << std::endl;
            }
            continue;
        }

        if (token == "tb") {
            const auto tablebase = probe_tablebase();
            if (tablebase.has_value()) {
                output << "Tablebase WDL: " << tablebase->wdl << ", DTM: " << tablebase->dtm << std::endl;
            } else {
                output << "No tablebase entry available for the current position." << std::endl;
            }
            continue;
        }

        if (token == "load") {
            std::string path;
            command >> path;
#if defined(__cpp_exceptions)
            try {
                load_game_from_file(path);
                output << "Loaded PGN with " << game_.moves.size() << " moves." << std::endl;
            } catch (const std::exception& error) {
                output << "Failed to load PGN: " << error.what() << std::endl;
            }
#else
            std::string error;
            auto game = sirio::files::try_load_pgn_from_file(path, &error);
            if (!game.has_value()) {
                output << "Failed to load PGN: " << error << std::endl;
            } else {
                load_game(*game);
                output << "Loaded PGN with " << game_.moves.size() << " moves." << std::endl;
            }
#endif
            continue;
        }

        if (token == "game") {
            if (game_.tags.empty() && game_.moves.empty()) {
                output << "No PGN game loaded." << std::endl;
            } else {
                output << "PGN tags:\n";
                for (const auto& [key, value] : game_.tags) {
                    output << "  " << key << ": " << value << '\n';
                }
                output << "Moves: " << game_.moves.size() << '\n';
                if (!game_.moves.empty()) {
                    output << "First moves: ";
                    for (std::size_t index = 0; index < std::min<std::size_t>(game_.moves.size(), 10); ++index) {
                        if (index != 0) {
                            output << ' ';
                        }
                        output << game_.moves[index];
                    }
                    output << std::endl;
                }
                output << "Result: " << game_.result << std::endl;
            }
            continue;
        }

        output << "Unknown command: " << token << std::endl;
    }
}

}  // namespace sirio::pyrrhic
