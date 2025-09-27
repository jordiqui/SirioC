#include "engine/uci/uci.hpp"
#include "engine/config.hpp"
#include "engine/core/board.hpp"
#include "engine/core/fen.hpp"
#include "engine/search/search.hpp"
#include "engine/eval/nnue/evaluator.hpp"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <iostream>
#include <sstream>
#include <vector>

namespace engine {

static Board g_board;
static Search g_search;
static nnue::Evaluator g_eval;
static bool g_use_nnue = true;
static std::string g_eval_file = "nn-000000.nnue";
static int g_hash_mb = 64;
static int g_threads = 1;
static bool g_stop = false;

static void try_load_network();

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
    std::cout << "id author Your Name\n";
    std::cout << "option name Hash type spin default 64 min 1 max 4096\n";
    std::cout << "option name Threads type spin default 1 min 1 max 256\n";
    std::cout << "option name UseNNUE type check default true\n";
    std::cout << "option name EvalFile type string default nn-000000.nnue\n";
    std::cout << "uciok\n" << std::flush;
}

void Uci::cmd_isready() {
    std::cout << "readyok\n" << std::flush;
}

void Uci::cmd_ucinewgame() {
    g_stop = false;
    g_board.set_startpos();
    // (Re)load network optionally
    try_load_network();
}

static void try_load_network() {
    if (!g_use_nnue) {
        return;
    }
    if (g_eval.load_network(g_eval_file)) {
        std::cout << "info string Loaded NNUE network from " << g_eval_file << "\n";
    } else {
        std::cout << "info string Failed to load NNUE network '" << g_eval_file
                  << "': " << g_eval.last_error() << "\n";
    }
}

static std::vector<std::string> split(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> out;
    std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

template <typename T>
static bool parse_integer(const std::string& value, T& out) {
    T tmp{};
    auto* begin = value.data();
    auto* end = value.data() + value.size();
    auto result = std::from_chars(begin, end, tmp);
    if (result.ec != std::errc() || result.ptr != end) {
        return false;
    }
    out = tmp;
    return true;
}

void Uci::cmd_setoption(const std::string& s) {
    // setoption name X value Y
    auto t = split(s);
    for (size_t i = 0; i < t.size(); ++i) {
        if (t[i] != "name" || i + 1 >= t.size()) {
            continue;
        }
        const std::string option_name = t[i + 1];
        size_t value_idx = i + 2;
        while (value_idx < t.size() && t[value_idx] != "value") {
            ++value_idx;
        }

        std::string value;
        if (value_idx < t.size()) {
            value_idx += 1; // skip "value"
            for (size_t k = value_idx; k < t.size(); ++k) {
                if (!value.empty()) {
                    value.push_back(' ');
                }
                value += t[k];
            }
        }

        if (option_name == "Hash") {
            int parsed = g_hash_mb;
            if (!value.empty() && parse_integer(value, parsed) && parsed >= 1 && parsed <= 4096) {
                g_hash_mb = parsed;
            } else {
                std::cout << "info string Invalid Hash value '" << value << "'\n";
            }
        } else if (option_name == "Threads") {
            int parsed = g_threads;
            if (!value.empty() && parse_integer(value, parsed) && parsed >= 1 && parsed <= 256) {
                g_threads = parsed;
            } else {
                std::cout << "info string Invalid Threads value '" << value << "'\n";
            }
        } else if (option_name == "UseNNUE") {
            std::string lower = value;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            const bool enable = (lower == "true" || lower == "1" || lower == "on");
            g_use_nnue = enable;
            if (enable) {
                try_load_network();
            } else {
                std::cout << "info string NNUE disabled\n";
            }
        } else if (option_name == "EvalFile") {
            if (!value.empty()) {
                g_eval_file = value;
                try_load_network();
            }
        }
        break;
    }
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

    // Minimal output for GUI friendliness
    std::cout << "info depth 1 score cp 0 nodes 1 time 0 pv" << "\n" << std::flush;

    // Until movegen/search exists, we cannot choose a legal move.
    std::cout << "bestmove (none)\n" << std::flush;
}

void Uci::cmd_stop() { g_stop = true; }

} // namespace engine
