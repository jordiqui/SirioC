#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "sirio/board.hpp"
#include "sirio/evaluation.hpp"
#include "sirio/move.hpp"
#include "sirio/movegen.hpp"
#include "sirio/nnue/api.hpp"
#include "sirio/search.hpp"
#include "sirio/syzygy.hpp"

namespace {

constexpr char kDefaultEvalFile[] = "nn-1c0000000000.nnue";
constexpr char kDefaultEvalFileSmall[] = "nn-37f18f62d772.nnue";

std::string pending_eval_file{kDefaultEvalFile};
std::string pending_eval_file_small{kDefaultEvalFileSmall};
std::ofstream debug_log_stream;
std::streambuf *original_clog_buffer = nullptr;

struct EngineOptions {
    std::string debug_log_file;
    std::string numa_policy = "auto";
    int threads = 1;
    std::size_t hash_size_mb = 16;
    bool ponder = false;
    int multi_pv = 1;
    int analysis_lines = 1;
    int skill_level = 20;
    int move_overhead = 10;
    int minimum_thinking_time = 100;
    int slow_mover = 100;
    int nodestime = 0;
    bool uci_chess960 = false;
    bool uci_limit_strength = false;
    int uci_elo = 1320;
    bool uci_show_wdl = false;
    std::string syzygy_path;
    int syzygy_probe_depth = 1;
    bool syzygy_50_move_rule = true;
    int syzygy_probe_limit = 7;
};

EngineOptions options;

void apply_time_management_options() {
    sirio::set_move_overhead(options.move_overhead);
    sirio::set_minimum_thinking_time(options.minimum_thinking_time);
    sirio::set_slow_mover(options.slow_mover);
    sirio::set_nodestime(options.nodestime);
}

void initialize_engine_options() {
    if (original_clog_buffer == nullptr) {
        original_clog_buffer = std::clog.rdbuf();
    }
    sirio::set_search_threads(options.threads);
    sirio::set_transposition_table_size(options.hash_size_mb);
    apply_time_management_options();
    sirio::syzygy::set_probe_depth_limit(options.syzygy_probe_depth);
    sirio::syzygy::set_probe_piece_limit(options.syzygy_probe_limit);
    sirio::syzygy::set_use_fifty_move_rule(options.syzygy_50_move_rule);
}

std::string normalize_eval_path(std::string value) {
    if (value == "<empty>") {
        return std::string{};
    }
    return value;
}

std::string normalize_string_option(const std::string &value) {
    if (value == "<empty>") {
        return std::string{};
    }
    return value;
}

bool parse_boolean_option(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value == "true" || value == "1" || value == "on" || value == "yes";
}

std::optional<int> parse_int_option(const std::string &value) {
    try {
        return std::stoi(value);
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

std::optional<std::size_t> parse_size_option(const std::string &value) {
    try {
        long long parsed = std::stoll(value);
        if (parsed < 0) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(parsed);
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

void print_loaded_nnue_info(const sirio::nnue::NetworkInfo &info) {
    std::cout << "info string NNUE evaluation using " << info.path;
    if (info.bytes != 0) {
        std::cout << " (" << (info.bytes / 1024 / 1024) << "MiB";
        if (!info.dims.empty()) {
            std::cout << ", " << info.dims;
        }
        std::cout << ")";
    } else if (!info.dims.empty()) {
        std::cout << " (" << info.dims << ")";
    }
    std::cout << std::endl;
}

bool nnue_try_load(const std::string &path, sirio::Board &board) {
    if (path.empty()) {
        return false;
    }

    std::string error;
    if (!sirio::nnue::init(path, &error)) {
        std::cout << "info string Failed to load NNUE: " << error << std::endl;
        return false;
    }

    sirio::initialize_evaluation(board);
    if (auto meta = sirio::nnue::info()) {
        print_loaded_nnue_info(*meta);
    } else {
        std::cout << "info string NNUE loaded from " << path << std::endl;
    }
    return true;
}

void nnue_load_if_pending(sirio::Board &board) {
    if (sirio::nnue::is_loaded()) {
        return;
    }
    if (!pending_eval_file.empty()) {
        if (nnue_try_load(pending_eval_file, board)) {
            return;
        }
    }
    if (!pending_eval_file_small.empty()) {
        nnue_try_load(pending_eval_file_small, board);
    }
}

void send_uci_id() {
    std::cout << "id name SirioC" << std::endl;
    std::cout << "id author Jorge Ruiz Centelles" << std::endl;
    std::cout << "option name Debug Log File type string default <empty>" << std::endl;
    std::cout << "option name NumaPolicy type string default auto" << std::endl;
    std::cout << "option name Threads type spin default 1 min 1 max 1024" << std::endl;
    std::cout << "option name Hash type spin default 16 min 1 max 33554432" << std::endl;
    std::cout << "option name Clear Hash type button" << std::endl;
    std::cout << "option name Ponder type check default false" << std::endl;
    std::cout << "option name MultiPV type spin default 1 min 1 max 256" << std::endl;
    std::cout << "option name Analysis Lines type spin default 1 min 1 max 256" << std::endl;
    std::cout << "option name Skill Level type spin default 20 min 0 max 20" << std::endl;
    std::cout << "option name Move Overhead type spin default 10 min 0 max 5000"
              << std::endl;
    std::cout << "option name Minimum Thinking Time type spin default 100 min 0 max 5000"
              << std::endl;
    std::cout << "option name Slow Mover type spin default 100 min 10 max 1000" << std::endl;
    std::cout << "option name nodestime type spin default 0 min 0 max 10000" << std::endl;
    std::cout << "option name UCI_Chess960 type check default false" << std::endl;
    std::cout << "option name UCI_LimitStrength type check default false" << std::endl;
    std::cout << "option name UCI_Elo type spin default 1320 min 1320 max 3190" << std::endl;
    std::cout << "option name UCI_ShowWDL type check default false" << std::endl;
    std::cout << "option name SyzygyPath type string default <empty>" << std::endl;
    std::cout << "option name SyzygyProbeDepth type spin default 1 min 1 max 100"
              << std::endl;
    std::cout << "option name Syzygy50MoveRule type check default true" << std::endl;
    std::cout << "option name SyzygyProbeLimit type spin default 7 min 0 max 7" << std::endl;
    std::cout << "option name EvalFile type string default " << kDefaultEvalFile << std::endl;
    std::cout << "option name EvalFileSmall type string default " << kDefaultEvalFileSmall
              << std::endl;
    std::cout << "uciok" << std::endl;
}

void send_ready(sirio::Board &board) {
    nnue_load_if_pending(board);
    std::cout << "readyok" << std::endl;
}

std::string trim_leading(std::string_view view) {
    std::size_t pos = 0;
    while (pos < view.size() && std::isspace(static_cast<unsigned char>(view[pos]))) {
        ++pos;
    }
    return std::string{view.substr(pos)};
}

std::string trim_whitespace(std::string_view view) {
    std::size_t start = 0;
    while (start < view.size() && std::isspace(static_cast<unsigned char>(view[start]))) {
        ++start;
    }
    std::size_t end = view.size();
    while (end > start && std::isspace(static_cast<unsigned char>(view[end - 1]))) {
        --end;
    }
    return std::string{view.substr(start, end - start)};
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
            sirio::initialize_evaluation(board);
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
            sirio::initialize_evaluation(board);
            return;
        }
    } else {
        throw std::runtime_error("Unsupported position command");
    }

    if (token != "moves") {
        while (stream >> token) {
            if (token == "moves") {
                break;
            }
        }
    }

    if (token != "moves") {
        sirio::initialize_evaluation(board);
        return;
    }

    while (stream >> token) {
        try {
            sirio::Move move = sirio::move_from_uci(board, token);
            board = board.apply_move(move);
        } catch (const std::exception &) {
            break;
        }
    }

    sirio::initialize_evaluation(board);
}

void handle_setoption(const std::string &args, sirio::Board &board) {
    std::istringstream stream{args};
    std::string token;
    std::string name;
    std::string value;
    while (stream >> token) {
        if (token == "name") {
            while (stream >> token) {
                if (token == "value") {
                    std::string remainder;
                    std::getline(stream, remainder);
                    value = trim_leading(remainder);
                    break;
                }
                if (!name.empty()) {
                    name += ' ';
                }
                name += token;
            }
            if (token != "value") {
                value.clear();
            }
            break;
        }
    }

    if (name.empty()) {
        return;
    }

    const std::string normalized_value = trim_whitespace(value);

    if (name == "Debug Log File") {
        std::string path = normalize_string_option(normalized_value);
        if (debug_log_stream.is_open()) {
            debug_log_stream.close();
        }
        if (!path.empty()) {
            debug_log_stream.open(path, std::ios::out | std::ios::app);
            if (debug_log_stream) {
                options.debug_log_file = path;
                if (original_clog_buffer == nullptr) {
                    original_clog_buffer = std::clog.rdbuf();
                }
                std::clog.rdbuf(debug_log_stream.rdbuf());
                std::cout << "info string Debug log file set to " << path << std::endl;
            } else {
                options.debug_log_file.clear();
                if (original_clog_buffer != nullptr) {
                    std::clog.rdbuf(original_clog_buffer);
                }
                std::cout << "info string Failed to open debug log file: " << path << std::endl;
            }
        } else {
            options.debug_log_file.clear();
            if (original_clog_buffer != nullptr) {
                std::clog.rdbuf(original_clog_buffer);
            }
        }
    } else if (name == "NumaPolicy") {
        options.numa_policy = normalized_value;
    } else if (name == "Threads") {
        if (auto parsed = parse_int_option(normalized_value); parsed.has_value()) {
            options.threads = std::clamp(*parsed, 1, 1024);
            sirio::set_search_threads(options.threads);
        }
    } else if (name == "Hash") {
        if (auto parsed = parse_size_option(normalized_value); parsed.has_value()) {
            std::size_t clamped = std::clamp<std::size_t>(*parsed, 1, 33'554'432);
            options.hash_size_mb = clamped;
            sirio::set_transposition_table_size(options.hash_size_mb);
        }
    } else if (name == "Clear Hash") {
        sirio::clear_transposition_tables();
        std::cout << "info string Transposition table cleared" << std::endl;
    } else if (name == "Ponder") {
        options.ponder = parse_boolean_option(normalized_value);
    } else if (name == "MultiPV") {
        if (auto parsed = parse_int_option(normalized_value); parsed.has_value()) {
            options.multi_pv = std::clamp(*parsed, 1, 256);
            options.analysis_lines = options.multi_pv;
        }
    } else if (name == "Analysis Lines") {
        if (auto parsed = parse_int_option(normalized_value); parsed.has_value()) {
            options.analysis_lines = std::clamp(*parsed, 1, 256);
            options.multi_pv = options.analysis_lines;
        }
    } else if (name == "Skill Level") {
        if (auto parsed = parse_int_option(normalized_value); parsed.has_value()) {
            options.skill_level = std::clamp(*parsed, 0, 20);
        }
    } else if (name == "Move Overhead") {
        if (auto parsed = parse_int_option(normalized_value); parsed.has_value()) {
            options.move_overhead = std::clamp(*parsed, 0, 5000);
            apply_time_management_options();
        }
    } else if (name == "Minimum Thinking Time") {
        if (auto parsed = parse_int_option(normalized_value); parsed.has_value()) {
            options.minimum_thinking_time = std::clamp(*parsed, 0, 5000);
            apply_time_management_options();
        }
    } else if (name == "Slow Mover") {
        if (auto parsed = parse_int_option(normalized_value); parsed.has_value()) {
            options.slow_mover = std::clamp(*parsed, 10, 1000);
            apply_time_management_options();
        }
    } else if (name == "nodestime") {
        if (auto parsed = parse_int_option(normalized_value); parsed.has_value()) {
            options.nodestime = std::clamp(*parsed, 0, 10000);
            apply_time_management_options();
        }
    } else if (name == "UCI_Chess960") {
        options.uci_chess960 = parse_boolean_option(normalized_value);
    } else if (name == "UCI_LimitStrength") {
        options.uci_limit_strength = parse_boolean_option(normalized_value);
    } else if (name == "UCI_Elo") {
        if (auto parsed = parse_int_option(normalized_value); parsed.has_value()) {
            options.uci_elo = std::clamp(*parsed, 1320, 3190);
        }
    } else if (name == "UCI_ShowWDL") {
        options.uci_show_wdl = parse_boolean_option(normalized_value);
    } else if (name == "SyzygyPath") {
        std::string path = normalize_string_option(normalized_value);
        options.syzygy_path = path;
        sirio::syzygy::set_tablebase_path(path);
    } else if (name == "SyzygyProbeDepth") {
        if (auto parsed = parse_int_option(normalized_value); parsed.has_value()) {
            options.syzygy_probe_depth = std::clamp(*parsed, 1, 100);
            sirio::syzygy::set_probe_depth_limit(options.syzygy_probe_depth);
        }
    } else if (name == "Syzygy50MoveRule") {
        options.syzygy_50_move_rule = parse_boolean_option(normalized_value);
        sirio::syzygy::set_use_fifty_move_rule(options.syzygy_50_move_rule);
    } else if (name == "SyzygyProbeLimit") {
        if (auto parsed = parse_int_option(normalized_value); parsed.has_value()) {
            options.syzygy_probe_limit = std::clamp(*parsed, 0, 7);
            sirio::syzygy::set_probe_piece_limit(options.syzygy_probe_limit);
        }
    } else if (name == "EvalFile") {
        pending_eval_file = normalize_eval_path(normalized_value);
        if (pending_eval_file.empty()) {
            sirio::nnue::unload();
            sirio::initialize_evaluation(board);
            std::cout << "info string NNUE evaluation disabled" << std::endl;
        } else {
            nnue_try_load(pending_eval_file, board);
        }
    } else if (name == "EvalFileSmall") {
        pending_eval_file_small = normalize_eval_path(normalized_value);
        nnue_try_load(pending_eval_file_small, board);
    } else if (name == "UseNNUE" || name == "NNUEFile") {
        if (name == "UseNNUE") {
            if (!parse_boolean_option(normalized_value)) {
                pending_eval_file.clear();
                pending_eval_file_small.clear();
                sirio::nnue::unload();
                sirio::initialize_evaluation(board);
                std::cout << "info string NNUE evaluation disabled" << std::endl;
            }
        } else {
            pending_eval_file = normalize_eval_path(normalized_value);
            if (pending_eval_file.empty()) {
                sirio::nnue::unload();
                sirio::initialize_evaluation(board);
                std::cout << "info string NNUE evaluation disabled" << std::endl;
            } else {
                nnue_try_load(pending_eval_file, board);
            }
        }
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

    sirio::initialize_evaluation(board);
    sirio::SearchResult result = sirio::search_best_move(board, limits);
    if (result.has_move) {
        int reported_depth = result.depth_reached > 0 ? result.depth_reached : limits.max_depth;
        std::cout << "info depth " << reported_depth << " score cp " << result.score;
        if (result.nodes > 0) {
            std::cout << " nodes " << result.nodes;
        }
        std::cout << " pv " << sirio::move_to_uci(result.best_move) << std::endl;
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

void handle_bench() {
    auto log = [](const std::string &message) {
        std::cout << "info string " << message << std::endl;
    };

    std::vector<std::string> speed_positions = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r1bq1rk1/ppp2ppp/2n2n2/3pp3/3P4/2P1PN2/PP1NBPPP/R2QKB1R w KQ - 0 7",
        "3r2k1/pp3ppp/2n1b3/3p4/3P4/2P1BN2/PP3PPP/3R2K1 w - - 0 1"};

    sirio::SearchLimits speed_limits;
    speed_limits.max_depth = 4;

    std::uint64_t total_nodes = 0;
    auto speed_start = std::chrono::steady_clock::now();
    for (const auto &fen : speed_positions) {
        sirio::Board bench_board{fen};
        auto result = sirio::search_best_move(bench_board, speed_limits);
        total_nodes += result.nodes;
    }
    auto speed_end = std::chrono::steady_clock::now();
    auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(speed_end - speed_start);
    double seconds = static_cast<double>(elapsed_ms.count()) / 1000.0;
    std::uint64_t nps = seconds > 0.0
                            ? static_cast<std::uint64_t>(static_cast<double>(total_nodes) / seconds)
                            : 0ULL;

    log("Search speed benchmark:");
    log("  Positions: " + std::to_string(speed_positions.size()));
    log("  Time: " + std::to_string(elapsed_ms.count()) + " ms");
    log("  Nodes: " + std::to_string(total_nodes));
    log("  Nodes per second: " + std::to_string(nps));

    struct TacticalPosition {
        std::string fen;
        std::string best_move;
    };

    std::vector<TacticalPosition> tactical_suite = {
        {"6k1/5ppp/8/6Q1/8/8/8/6K1 w - - 0 1", "g5d8"},
        {"k7/8/8/8/8/8/5PPP/6KQ w - - 0 1", "g2g4"}};

    sirio::SearchLimits tactic_limits;
    tactic_limits.max_depth = 1;
    tactic_limits.move_time = 1000;

    int correct = 0;
    std::vector<std::string> mismatch_logs;
    for (const auto &entry : tactical_suite) {
        sirio::Board bench_board{entry.fen};
        auto result = sirio::search_best_move(bench_board, tactic_limits);
        std::string uci = result.has_move ? sirio::move_to_uci(result.best_move) : "(none)";
        if (result.has_move && uci == entry.best_move) {
            ++correct;
        } else {
            mismatch_logs.push_back("  " + entry.fen + " -> esperado " + entry.best_move +
                                   ", obtenido " + uci);
        }
    }

    log("Tactical suite accuracy: " + std::to_string(correct) + "/" +
        std::to_string(tactical_suite.size()));
    for (const auto &line : mismatch_logs) {
        log(line);
    }

    const std::string &tb_path = sirio::syzygy::tablebase_path();
    if (!tb_path.empty() && sirio::syzygy::available()) {
        sirio::Board tb_board{"8/8/8/8/8/6k1/6P1/6K1 w - - 0 1"};
        if (auto probe = sirio::syzygy::probe_root(tb_board); probe.has_value() &&
                                                             probe->best_move.has_value()) {
            log(std::string{"Syzygy probe move: "} +
                sirio::move_to_uci(*probe->best_move) + " (wdl=" +
                std::to_string(probe->wdl) + ", dtz=" + std::to_string(probe->dtz) + ")");
        }
    } else {
        log("Syzygy tablebases no configuradas. Establezca la opción SyzygyPath para habilitar "
            "las pruebas de tablebases.");
    }

    log("Bench finalizado");
}

}  // namespace

int main() {
    sirio::Board board;
    sirio::initialize_evaluation(board);
    initialize_engine_options();
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
                send_ready(board);
            } else if (command == "ucinewgame") {
                board = sirio::Board{};
                sirio::initialize_evaluation(board);
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
            } else if (command == "bench") {
                handle_bench();
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

