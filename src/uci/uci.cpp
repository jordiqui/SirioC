#include "engine/uci/uci.hpp"
#include "engine/config.hpp"
#include "engine/core/board.hpp"
#include "engine/core/fen.hpp"
#include "engine/core/perft.hpp"
#include "engine/search/search.hpp"
#include "engine/eval/nnue/evaluator.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

namespace engine {

static Board g_board;
static Search g_search;
static nnue::Evaluator g_eval;
static bool g_use_nnue = true;
static std::string g_eval_file = "nn-1c0000000000.nnue";
static std::string g_eval_file_small = "nn-37f18f62d772.nnue";
static int g_hash_mb = 64;
namespace {
constexpr int kThreadsMax = 256;

int detect_default_thread_count() {
    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) return 1;
    if (hw > static_cast<unsigned>(kThreadsMax)) return kThreadsMax;
    return static_cast<int>(hw);
}
} // namespace

static const int g_default_threads = detect_default_thread_count();
static int g_threads = g_default_threads;
static int g_numa_offset = 0;
static bool g_ponder = true;
static int g_multi_pv = 1;
static int g_move_overhead = 10;
static bool g_stop = false;
static bool g_use_syzygy = false;
static std::string g_syzygy_path;

static void ensure_nnue_loaded() {
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

static void sync_search_options() {
    ensure_nnue_loaded();
    g_search.set_hash(g_hash_mb);
    g_search.set_threads(g_threads);
    g_search.set_numa_offset(g_numa_offset);
    g_search.set_ponder(g_ponder);
    g_search.set_multi_pv(g_multi_pv);
    g_search.set_move_overhead(g_move_overhead);
    g_search.set_eval_file(g_eval_file);
    g_search.set_eval_file_small(g_eval_file_small);
    g_search.set_use_syzygy(g_use_syzygy);
    g_search.set_syzygy_path(g_syzygy_path);
    g_search.set_use_nnue(g_use_nnue);
    g_search.set_nnue_evaluator(g_use_nnue ? &g_eval : nullptr);
}

void Uci::loop() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        handle_line(line);
        if (line == "quit") break;
    }
}

void Uci::handle_line(const std::string& line) {
    if (line == "uci") return cmd_uci();
    if (line == "isready") return cmd_isready();
    if (line == "ucinewgame") return cmd_ucinewgame();
    if (line.rfind("setoption",0)==0) return cmd_setoption(line);
    if (line.rfind("position",0)==0) return cmd_position(line);
    if (line.rfind("perft",0)==0) return cmd_perft(line);
    if (line.rfind("bench",0)==0) return cmd_bench(line);
    if (line.rfind("go",0)==0) return cmd_go(line);
    if (line == "stop") return cmd_stop();
    if (line == "quit") { /* handled in loop */ }
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
    std::cout << "uciok\n" << std::flush;
}

void Uci::cmd_isready() {
    std::cout << "readyok\n" << std::flush;
}

void Uci::cmd_ucinewgame() {
    g_stop = false;
    g_board.set_startpos();
    ensure_nnue_loaded();
    sync_search_options();
}

static std::vector<std::string> split(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> out;
    std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

void Uci::cmd_setoption(const std::string& s) {
    // setoption name X value Y
    auto t = split(s);
    // naive parse
    for (size_t i = 0; i < t.size(); ++i) {
        if (t[i] == "name" && i + 1 < t.size()) {
            size_t name_start = i + 1;
            size_t value_pos = t.size();
            for (size_t j = name_start; j < t.size(); ++j) {
                if (t[j] == "value") {
                    value_pos = j;
                    break;
                }
            }

            std::string name;
            for (size_t j = name_start; j < value_pos && j < t.size(); ++j) {
                if (!name.empty()) name.push_back(' ');
                name += t[j];
            }

            std::string value;
            if (value_pos < t.size() && value_pos + 1 < t.size()) {
                value = t[value_pos + 1];
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
            break;
        }
    }
    sync_search_options();
}

void Uci::cmd_position(const std::string& s) {
    // position [startpos | fen <FEN>] [moves ...]
    std::string rest = s.substr(8);
    auto t = split(rest);
    size_t i=0;
    if (i<t.size() && t[i]=="startpos") {
        g_board.set_startpos();
        ++i;
    } else if (i<t.size() && t[i]=="fen") {
        std::string fen;
        ++i;
        // FEN spans up to next 'moves' or end
        while (i<t.size() && t[i]!="moves") {
            if (!fen.empty()) fen.push_back(' ');
            fen += t[i++];
        }
        if (!fen::is_valid_fen(fen) || !g_board.set_fen(fen)) {
            // accept silently but keep previous
        }
    }

    if (i<t.size() && t[i]=="moves") {
        ++i;
        std::vector<std::string> moves;
        for (; i<t.size(); ++i) moves.push_back(t[i]);
        g_board.apply_moves_uci(moves);
    }
}

static Limits parse_go_tokens(const std::vector<std::string>& t) {
    Limits lim;
    for (size_t i=0;i<t.size();++i) {
        if (t[i]=="depth" && i+1<t.size()) lim.depth = std::stoi(t[i+1]);
        else if (t[i]=="movetime" && i+1<t.size()) lim.movetime_ms = std::stoll(t[i+1]);
        else if (t[i]=="wtime" && i+1<t.size()) lim.wtime_ms = std::stoll(t[i+1]);
        else if (t[i]=="btime" && i+1<t.size()) lim.btime_ms = std::stoll(t[i+1]);
        else if (t[i]=="winc" && i+1<t.size()) lim.winc_ms = std::stoll(t[i+1]);
        else if (t[i]=="binc" && i+1<t.size()) lim.binc_ms = std::stoll(t[i+1]);
        else if (t[i]=="nodes" && i+1<t.size()) lim.nodes = std::stoll(t[i+1]);
    }
    return lim;
}

void Uci::cmd_go(const std::string& s) {
    g_stop = false;
    auto t = split(s);
    Limits lim = parse_go_tokens(t);
    sync_search_options();

    auto format_score = [](int score) {
        constexpr int kMateValue = 30000;
        constexpr int kMateThreshold = 29000;
        if (std::abs(score) >= kMateThreshold) {
            int mate = (kMateValue - std::abs(score) + 1) / 2;
            if (score < 0) mate = -mate;
            return std::string("mate ") + std::to_string(mate);
        }
        return std::string("cp ") + std::to_string(score);
    };

    g_search.set_info_callback([&](const Search::Info& info) {
        std::cout << "info depth " << info.depth << " score " << format_score(info.score)
                  << " nodes " << info.nodes << " time " << info.time_ms << " pv";
        Board pv_board = g_board;
        std::vector<Board::State> pv_states;
        pv_states.reserve(info.pv.size());
        for (Move mv : info.pv) {
            std::string uci = pv_board.move_to_uci(mv);
            if (uci == "0000") break;
            std::cout << ' ' << uci;
            Board::State pv_state;
            pv_board.apply_move(mv, pv_state);
            pv_states.push_back(pv_state);
        }
        for (auto it = pv_states.rbegin(); it != pv_states.rend(); ++it) {
            pv_board.undo_move(*it);
        }
        std::cout << "\n" << std::flush;
    });

    auto result = g_search.find_bestmove(g_board, lim);
    g_search.set_info_callback(nullptr);
    std::string best_uci = g_board.move_to_uci(result.bestmove);

    if (result.depth == 0 && result.pv.empty() && result.bestmove == MOVE_NONE) {
        std::cout << "info depth 0 score cp 0 nodes " << result.nodes << " time " << result.time_ms
                  << " pv\n";
    }

    if (result.bestmove == MOVE_NONE) {
        std::cout << "bestmove (none)\n";
    } else {
        std::cout << "bestmove " << best_uci << "\n";
    }
    std::cout << std::flush;
}

void Uci::cmd_perft(const std::string& s) {
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
        Board::State state;
        g_board.apply_move(move, state);
        uint64_t count = perft(g_board, depth - 1);
        g_board.undo_move(state);
        total += count;
        std::cout << uci << ": " << count << "\n";
    }
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start)
                           .count();
    uint64_t nps = elapsed_ms > 0 ? total * 1000ULL / static_cast<uint64_t>(elapsed_ms) : 0;
    std::cout << "total: " << total << " time " << elapsed_ms << " nps " << nps << "\n";
}

void Uci::cmd_bench(const std::string& s) {
    auto tokens = split(s);
    int depth = 6;
    int requested_threads = -1;
    for (size_t i = 1; i < tokens.size(); ++i) {
        if (tokens[i] == "depth" && i + 1 < tokens.size()) {
            char* end = nullptr;
            long val = std::strtol(tokens[i + 1].c_str(), &end, 10);
            if (end != tokens[i + 1].c_str()) depth = static_cast<int>(val);
            ++i;
        } else if (tokens[i] == "threads" && i + 1 < tokens.size()) {
            char* end = nullptr;
            long val = std::strtol(tokens[i + 1].c_str(), &end, 10);
            if (end != tokens[i + 1].c_str()) requested_threads = static_cast<int>(val);
            ++i;
        }
    }
    if (depth < 1) depth = 1;

    int bench_threads = requested_threads > 0 ? requested_threads : g_threads;
    if (bench_threads <= 1 && requested_threads <= 0) {
        bench_threads = g_default_threads;
    }
    bench_threads = std::clamp(bench_threads, 1, kThreadsMax);

    static const std::vector<std::string> bench_fens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/pp1bbppp/2n1pn2/q1pp4/3P4/2P1PN2/PPBN1PPP/R2QKB1R w KQkq - 4 9",
        "r1bq1rk1/pp3pbp/2n1pnp1/3p4/3P4/2N1PN2/PPQ1BPPP/R1B2RK1 w - - 0 9",
        "2rq1rk1/pp1b1pp1/2n1pn1p/2pp4/3P4/2P1PN2/PP1N1PPP/2RQ1RK1 w - - 0 10",
        "r1bq1rk1/1p1nbppp/p2ppn2/2p5/2PP4/1PN1PN2/PB2BPPP/R2Q1RK1 w - - 0 9",
        "2r2rk1/pp1bbppp/2n2n2/q2pp3/3P4/2N1PN2/PPQ1BPPP/R3KB1R w KQ - 4 11"
    };

    sync_search_options();
    if (bench_threads != g_threads) {
        g_search.set_threads(bench_threads);
    }

    Limits lim;
    lim.depth = depth;
    lim.movetime_ms = -1;
    uint64_t total_nodes = 0;
    auto start = std::chrono::steady_clock::now();

    for (const auto& fen : bench_fens) {
        Board board;
        board.set_fen(fen);
        auto result = g_search.find_bestmove(board, lim);
        total_nodes += result.nodes;
    }

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start)
                           .count();
    uint64_t nps = elapsed_ms > 0 ? total_nodes * 1000ULL / static_cast<uint64_t>(elapsed_ms) : 0;
    std::cout << "bench positions " << bench_fens.size() << " depth " << depth
              << " threads " << bench_threads << " nodes " << total_nodes << " time " << elapsed_ms
              << " nps " << nps << "\n" << std::flush;

    if (bench_threads != g_threads) {
        g_search.set_threads(g_threads);
    }
}

void Uci::cmd_stop() {
    g_stop = true;
    g_search.stop();
}

} // namespace engine
