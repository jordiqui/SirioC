#include "engine/uci/uci.hpp"
#include "engine/config.hpp"
#include "engine/core/board.hpp"
#include "engine/core/fen.hpp"
#include "engine/search/search.hpp"
#include "engine/eval/nnue/evaluator.hpp"
#include <cmath>
#include <iostream>
#include <sstream>
#include <vector>

namespace engine {

static Board g_board;
static Search g_search;
static nnue::Evaluator g_eval;
static bool g_use_nnue = true;
static std::string g_eval_file = "nn-1c0000000000.nnue";
static std::string g_eval_file_small = "nn-37f18f62d772.nnue";
static int g_hash_mb = 64;
static int g_threads = 1;
static int g_numa_offset = 0;
static bool g_ponder = true;
static int g_multi_pv = 1;
static int g_move_overhead = 10;
static bool g_stop = false;
static bool g_use_syzygy = false;
static std::string g_syzygy_path;

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
    if (line.rfind("go",0)==0) return cmd_go(line);
    if (line == "stop") return cmd_stop();
    if (line == "quit") { /* handled in loop */ }
}

void Uci::cmd_uci() {
    std::cout << "id name " << engine::kEngineName << " " << engine::kEngineVersion << "\n";
    std::cout << "id author Jorge Ruiz, Codex ChatGPT\n";
    std::cout << "option name Hash type spin default 64 min 1 max 4096\n";
    std::cout << "option name Threads type spin default 1 min 1 max 256\n";
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
    // (Re)load network optionally
    if (g_use_nnue) g_eval.load_network(g_eval_file);
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
            else if (name == "Threads" && !value.empty()) g_threads = std::stoi(value);
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
    (void)lim;

    g_search.set_threads(g_threads);
    g_search.set_hash(g_hash_mb);
    g_search.set_numa_offset(g_numa_offset);
    g_search.set_ponder(g_ponder);
    g_search.set_multi_pv(g_multi_pv);
    g_search.set_move_overhead(g_move_overhead);
    g_search.set_eval_file(g_eval_file);
    g_search.set_eval_file_small(g_eval_file_small);
    g_search.set_use_syzygy(g_use_syzygy);
    g_search.set_syzygy_path(g_syzygy_path);

    auto result = g_search.find_bestmove(g_board, lim);
    std::string best_uci = g_board.move_to_uci(result.bestmove);

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

    std::cout << "info depth " << result.depth << " score " << format_score(result.score)
              << " nodes " << result.nodes << " time " << result.time_ms << " pv";
    Board pv_board = g_board;
    for (Move mv : result.pv) {
        std::string uci = pv_board.move_to_uci(mv);
        if (uci == "0000") break;
        std::cout << ' ' << uci;
        pv_board = pv_board.after_move(mv);
    }
    std::cout << "\n";

    if (result.bestmove == MOVE_NONE) {
        std::cout << "bestmove (none)\n";
    } else {
        std::cout << "bestmove " << best_uci << "\n";
    }
    std::cout << std::flush;
}

void Uci::cmd_stop() {
    g_stop = true;
    g_search.stop();
}

} // namespace engine
