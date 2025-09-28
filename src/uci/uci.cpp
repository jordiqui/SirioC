#include "engine/uci/uci.hpp"

#include "engine/bench/bench.hpp"

#include "engine/config.hpp"
#include "engine/core/board.hpp"
#include "engine/core/fen.hpp"
#include "engine/core/perft.hpp"
#include "engine/eval/nnue/evaluator.hpp"
#include "engine/search/search.hpp"
#include "engine/syzygy/syzygy.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>
#include <future>
#include <memory>
#if defined(_WIN32)
#    include <io.h>
#else
#    include <unistd.h>
#endif

namespace engine {

const std::vector<std::string>& bench_positions();

namespace {

Board g_board;
Search g_search;
nnue::Evaluator g_eval;

bool g_use_nnue = true;
std::string g_eval_file = "nn-1c0000000000.nnue";
std::string g_eval_file_small = "nn-37f18f62d772.nnue";
int g_hash_mb = 64;

constexpr int kThreadsMax = 256;

int detect_default_thread_count() {
    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) return 1;
    if (hw > static_cast<unsigned>(kThreadsMax)) return kThreadsMax;
    return static_cast<int>(hw);
}

const int g_default_threads = detect_default_thread_count();
int g_threads = g_default_threads;
int g_numa_offset = 0;
bool g_ponder = true;
int g_multi_pv = 1;
int g_move_overhead = 10;

bool g_use_syzygy = false;
std::string g_syzygy_path;
int g_syzygy_probe_depth = 1;
int g_syzygy_probe_limit = 7;
bool g_syzygy_rule50 = true;

struct UciLiteInfo {
    int depth = 0;
    int score = 0;
    uint64_t nodes = 0;
    uint64_t nps = 0;
    int time_ms = 0;
    std::string pv;
};

void ensure_nnue_loaded() {
    if (!g_use_nnue) return;
    if (!g_eval.loaded() || g_eval.loaded_path() != g_eval_file) {
        if (!g_eval.load_network(g_eval_file)) {
            std::cout << "info string Failed to load NNUE network '" << g_eval_file
                      << "', disabling UseNNUE\n" << std::flush;
            g_use_nnue = false;
        } else {
            std::cout << "info string Loaded NNUE network '" << g_eval_file << "'\n"
                      << std::flush;
        }
    }
}

void sync_search_options() {
    ensure_nnue_loaded();
    g_search.set_hash(g_hash_mb);
    g_search.set_threads(g_threads);
    g_search.set_numa_offset(g_numa_offset);
    g_search.set_ponder(g_ponder);
    g_search.set_multi_pv(g_multi_pv);
    g_search.set_move_overhead(g_move_overhead);
    g_search.set_eval_file(g_eval_file);
    g_search.set_eval_file_small(g_eval_file_small);

    syzygy::TBConfig tb_config;
    tb_config.enabled = g_use_syzygy;
    tb_config.path = g_syzygy_path;
    tb_config.probe_depth = g_syzygy_probe_depth;
    tb_config.probe_limit = g_syzygy_probe_limit;
    tb_config.use_rule50 = g_syzygy_rule50;
    g_search.set_syzygy_config(std::move(tb_config));

    g_search.set_use_nnue(g_use_nnue);
    g_search.set_nnue_evaluator(g_use_nnue ? &g_eval : nullptr);
}

std::vector<std::string> split(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> out;
    std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

Limits parse_go_tokens(const std::vector<std::string>& t) {
    Limits lim;
    for (size_t i = 0; i < t.size(); ++i) {
        if (t[i] == "depth" && i + 1 < t.size()) lim.depth = std::stoi(t[i + 1]);
        else if (t[i] == "movetime" && i + 1 < t.size()) lim.movetime_ms = std::stoll(t[i + 1]);
        else if (t[i] == "wtime" && i + 1 < t.size()) lim.wtime_ms = std::stoll(t[i + 1]);
        else if (t[i] == "btime" && i + 1 < t.size()) lim.btime_ms = std::stoll(t[i + 1]);
        else if (t[i] == "winc" && i + 1 < t.size()) lim.winc_ms = std::stoll(t[i + 1]);
        else if (t[i] == "binc" && i + 1 < t.size()) lim.binc_ms = std::stoll(t[i + 1]);
        else if (t[i] == "nodes" && i + 1 < t.size()) lim.nodes = std::stoll(t[i + 1]);
    }
    return lim;
}

std::string format_score(int score) {
    constexpr int kMateValue = 30000;
    constexpr int kMateThreshold = 29000;
    if (std::abs(score) >= kMateThreshold) {
        int mate = (kMateValue - std::abs(score) + 1) / 2;
        if (score < 0) mate = -mate;
        return std::string("mate ") + std::to_string(mate);
    }
    return std::string("cp ") + std::to_string(score);
}

std::string pv_to_string(const Board& base, const std::vector<Move>& pv) {
    if (pv.empty()) return {};
    Board copy = base;
    std::vector<Board::State> states;
    states.reserve(pv.size());
    std::string out;
    bool first = true;
    for (Move mv : pv) {
        if (mv == MOVE_NONE) break;
        std::string uci = copy.move_to_uci(mv);
        if (uci == "0000") break;
        if (!first) out.push_back(' ');
        first = false;
        out += uci;
        Board::State st;
        copy.apply_move(mv, st);
        states.push_back(st);
    }
    for (auto it = states.rbegin(); it != states.rend(); ++it) {
        copy.undo_move(*it);
    }
    return out;
}

bool stdin_is_terminal() {
#if defined(_WIN32)
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(fileno(stdin)) != 0;
#endif
}

void wait_for_enter_if_interactive() {
    if (!stdin_is_terminal()) return;
    std::cout << "info string SirioC espera comandos UCI desde una consola o GUI compatible.\n"
              << std::flush;
    std::cout << "info string Presiona Enter para cerrar...\n" << std::flush;
    std::cin.clear();
    std::string dummy;
    std::getline(std::cin, dummy);
}

} // namespace

void Uci::loop() {
    std::string line;
    bool processed_command = false;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        processed_command = true;
        handle_line(line);
        if (line == "quit") break;
    }
    stop_search_thread();
    if (!processed_command) {
        wait_for_enter_if_interactive();
    }
    syzygy::shutdown();
}

void Uci::handle_line(const std::string& line) {
    if (line == "uci") return cmd_uci();
    if (line == "isready") return cmd_isready();
    if (line == "ucinewgame") return cmd_ucinewgame();
    if (line.rfind("setoption", 0) == 0) return cmd_setoption(line);
    if (line.rfind("position", 0) == 0) return cmd_position(line);
    if (line.rfind("perft", 0) == 0) return cmd_perft(line);
    if (line.rfind("bench", 0) == 0) return cmd_bench(line);
    if (line.rfind("go", 0) == 0) return cmd_go(line);
    if (line == "stop") return cmd_stop();
    if (line == "quit") { /* handled by loop */ }
}

void Uci::cmd_uci() {
    std::cout << "id name " << engine::kEngineName << " " << engine::kEngineVersion << "\n";
    std::cout << "id author Jorge Ruiz creditos Codex OpenAi\n";
    std::cout << "option name Hash type spin default 64 min 1 max 4096\n";
    std::cout << "option name Threads type spin default " << g_default_threads
              << " min 1 max " << kThreadsMax << "\n";
    std::cout << "option name NUMA Offset type spin default 0 min -1 max 32\n";
    std::cout << "option name Ponder type check default true\n";
    std::cout << "option name MultiPV type spin default 1 min 1 max 218\n";
    std::cout << "option name UseNNUE type check default true\n";
    std::cout << "option name EvalFile type string default nn-1c0000000000.nnue\n";
    std::cout << "option name EvalFileSmall type string default nn-37f18f62d772.nnue\n";
    std::cout << "option name Move Overhead type spin default 10 min 0 max 5000\n";
    std::cout << "option name UseSyzygy type check default false\n";
    std::cout << "option name SyzygyPath type string default \"\"\n";
    std::cout << "option name SyzygyProbeDepth type spin default 1 min 0 max 128\n";
    std::cout << "option name SyzygyProbeLimit type spin default 7 min 0 max 7\n";
    std::cout << "option name Syzygy50MoveRule type check default true\n";
    std::cout << "uciok\n" << std::flush;
}

void Uci::cmd_isready() {
    stop_search_thread();
    ensure_nnue_loaded();
    sync_search_options();
    std::cout << "readyok\n" << std::flush;
}

void Uci::cmd_ucinewgame() {
    stop_search_thread();
    g_board.set_startpos();
    ensure_nnue_loaded();
    sync_search_options();
}

void Uci::cmd_setoption(const std::string& s) {
    stop_search_thread();
    auto tokens = split(s);
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i] == "name" && i + 1 < tokens.size()) {
            size_t name_start = i + 1;
            size_t value_pos = tokens.size();
            for (size_t j = name_start; j < tokens.size(); ++j) {
                if (tokens[j] == "value") {
                    value_pos = j;
                    break;
                }
            }

            std::string name;
            for (size_t j = name_start; j < value_pos && j < tokens.size(); ++j) {
                if (!name.empty()) name.push_back(' ');
                name += tokens[j];
            }

            std::string value;
            if (value_pos < tokens.size() && value_pos + 1 < tokens.size()) {
                value = tokens[value_pos + 1];
            }

            auto parse_bool = [](const std::string& v) {
                return v.empty() || v == "true" || v == "1" || v == "on";
            };

            if (name == "Hash" && !value.empty()) g_hash_mb = std::stoi(value);
            else if (name == "Threads" && !value.empty()) {
                int parsed = std::stoi(value);
                g_threads = std::clamp(parsed, 1, kThreadsMax);
            }
            else if (name == "NUMA Offset" && !value.empty()) g_numa_offset = std::stoi(value);
            else if (name == "Ponder") g_ponder = parse_bool(value);
            else if (name == "MultiPV" && !value.empty()) g_multi_pv = std::stoi(value);
            else if (name == "UseNNUE") g_use_nnue = parse_bool(value);
            else if (name == "EvalFile" && !value.empty()) g_eval_file = value;
            else if (name == "EvalFileSmall" && !value.empty()) g_eval_file_small = value;
            else if (name == "Move Overhead" && !value.empty()) g_move_overhead = std::stoi(value);
            else if (name == "UseSyzygy") g_use_syzygy = parse_bool(value);
            else if (name == "SyzygyPath" && !value.empty()) g_syzygy_path = value;
            else if (name == "SyzygyProbeDepth" && !value.empty()) {
                int parsed = std::stoi(value);
                g_syzygy_probe_depth = std::clamp(parsed, 0, 128);
            }
            else if (name == "SyzygyProbeLimit" && !value.empty()) {
                int parsed = std::stoi(value);
                g_syzygy_probe_limit = std::clamp(parsed, 0, 7);
            }
            else if (name == "Syzygy50MoveRule") g_syzygy_rule50 = parse_bool(value);

            break;
        }
    }
    sync_search_options();
}

void Uci::cmd_position(const std::string& s) {
    stop_search_thread();
    std::string rest = s.substr(8);
    auto tokens = split(rest);
    size_t idx = 0;
    if (idx < tokens.size() && tokens[idx] == "startpos") {
        g_board.set_startpos();
        ++idx;
    } else if (idx < tokens.size() && tokens[idx] == "fen") {
        std::string fen;
        ++idx;
        while (idx < tokens.size() && tokens[idx] != "moves") {
            if (!fen.empty()) fen.push_back(' ');
            fen += tokens[idx++];
        }
        if (fen::is_valid_fen(fen)) {
            g_board.set_fen(fen);
        }
    }

    if (idx < tokens.size() && tokens[idx] == "moves") {
        ++idx;
        std::vector<std::string> moves;
        for (; idx < tokens.size(); ++idx) moves.push_back(tokens[idx]);
        g_board.apply_moves_uci(moves);
    }
}

void Uci::cmd_go(const std::string& s) {
    stop_search_thread();
    auto tokens = split(s);
    Limits lim = parse_go_tokens(tokens);
    sync_search_options();
    Board board_copy = g_board;

    auto ready = std::make_shared<std::promise<void>>();
    auto ready_future = ready->get_future();

    search_thread_ = std::thread([this, lim, board_copy = std::move(board_copy), ready]() mutable {
        g_search.set_info_callback([&](const Search::Info& info) {
            UciLiteInfo lite;
            lite.depth = info.depth;
            lite.score = info.score;
            lite.nodes = info.nodes;
            lite.time_ms = info.time_ms;
            lite.nps = info.time_ms > 0
                           ? info.nodes * 1000ULL / static_cast<uint64_t>(info.time_ms)
                           : 0;
            lite.pv = pv_to_string(board_copy, info.pv);

            std::cout << "info depth " << lite.depth << " score " << format_score(lite.score)
                      << " nodes " << lite.nodes << " time " << lite.time_ms << " nps "
                      << lite.nps;
            if (!lite.pv.empty()) {
                std::cout << " pv " << lite.pv;
            }
            std::cout << "\n" << std::flush;
        });

        ready->set_value();

        auto result = g_search.find_bestmove(board_copy, lim);
        g_search.set_info_callback(nullptr);

        if (result.bestmove == MOVE_NONE) {
            // UCI requires engines with no legal moves to report "bestmove 0000"
            // (or "null") so that GUIs treat the search as finished instead of
            // assuming the engine disconnected. Emitting the literal string
            // "(none)" caused relay servers to drop SirioC after checkmates or
            // stalemates, producing forfeits despite the engine completing the
            // search. Use the standard 0000 placeholder to remain compliant.
            std::cout << "bestmove 0000\n";
        } else {
            std::cout << "bestmove " << board_copy.move_to_uci(result.bestmove) << "\n";
        }
        std::cout << std::flush;
    });

    ready_future.wait();
}

void Uci::cmd_stop() {
    stop_search_thread();
}

void Uci::cmd_perft(const std::string& s) {
    stop_search_thread();
    auto tokens = split(s);
    int depth = 1;
    bool divide = false;
    for (size_t i = 1; i < tokens.size(); ++i) {
        if (tokens[i] == "divide") {
            divide = true;
        } else {
            char* end = nullptr;
            long val = std::strtol(tokens[i].c_str(), &end, 10);
            if (end != tokens[i].c_str()) depth = static_cast<int>(val);
        }
    }

    if (depth <= 0) {
        std::cout << "perft depth " << depth << " nodes 1 time 0 nps 0\n";
        return;
    }

    auto start = std::chrono::steady_clock::now();
    if (!divide) {
        uint64_t nodes = perft(g_board, depth);
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - start)
                              .count();
        uint64_t nps = elapsed_ms > 0 ? nodes * 1000ULL / static_cast<uint64_t>(elapsed_ms) : 0;
        std::cout << "perft depth " << depth << " nodes " << nodes << " time " << elapsed_ms
                  << " nps " << nps << "\n";
        return;
    }

    auto moves = g_board.generate_legal_moves();
    uint64_t total = 0;
    for (Move move : moves) {
        std::string uci = g_board.move_to_uci(move);
        Board::State st;
        g_board.apply_move(move, st);
        uint64_t nodes = perft(g_board, depth - 1);
        g_board.undo_move(st);
        total += nodes;
        std::cout << uci << ": " << nodes << "\n";
    }
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - start)
                          .count();
    uint64_t nps = elapsed_ms > 0 ? total * 1000ULL / static_cast<uint64_t>(elapsed_ms) : 0;
    std::cout << "perft depth " << depth << " nodes " << total << " time " << elapsed_ms
              << " nps " << nps << "\n";
}

void Uci::cmd_bench(const std::string& s) {
    stop_search_thread();
    auto tokens = split(s);
    int depth = 8;
    int threads = g_threads;
    bool explicit_threads = false;
    bool perft_mode = false;
    for (size_t i = 1; i < tokens.size(); ++i) {
        if (tokens[i] == "depth" && i + 1 < tokens.size()) {
            depth = std::stoi(tokens[i + 1]);
            ++i;
        } else if (tokens[i] == "threads" && i + 1 < tokens.size()) {
            threads = std::stoi(tokens[i + 1]);
            explicit_threads = true;
            ++i;
        } else if (tokens[i] == "perft") {
            perft_mode = true;
        }
    }

    if (!explicit_threads && g_threads == 1) {
        threads = std::max(1, g_default_threads);
    }

    int previous_threads = g_threads;
    g_threads = std::clamp(threads, 1, kThreadsMax);
    sync_search_options();

    if (perft_mode) {
        auto result = bench::run_perft_suite(depth);
        uint64_t nps = result.time_ms > 0
                            ? result.nodes * 1000ULL / static_cast<uint64_t>(result.time_ms)
                            : 0;
        std::cout << "bench perft depth " << depth << " nodes " << result.nodes << " time "
                  << result.time_ms << " nps " << nps << " positions " << result.positions
                  << " verified " << (result.verified ? "true" : "false") << "\n";
    } else {
        auto result = bench::run(g_search, depth);
        uint64_t nps = result.time_ms > 0
                            ? result.nodes * 1000ULL / static_cast<uint64_t>(result.time_ms)
                            : 0;
        std::cout << "bench depth " << depth << " nodes " << result.nodes << " time "
                  << result.time_ms << " nps " << nps << " positions " << result.positions
                  << "\n";
    }

    g_threads = previous_threads;
    sync_search_options();
}

void Uci::stop_search_thread() {
    if (search_thread_.joinable()) {
        g_search.stop();
        search_thread_.join();
        g_search.set_info_callback(nullptr);
    }
}

} // namespace engine
