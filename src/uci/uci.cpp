#include "engine/uci/uci.hpp"
#include "engine/config.hpp"
#include "engine/core/board.hpp"
#include "engine/core/fen.hpp"
#include "engine/search/search.hpp"
#include "engine/eval/nnue/evaluator.hpp"
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
    for (size_t i=0;i<t.size();++i) {
        if (t[i]=="name" && i+1<t.size()) {
            std::string name = t[i+1];
            // find "value"
            size_t j = i+2;
            while (j<t.size() && t[j]!="value") ++j;
            std::string value;
            if (j+1<t.size()) value = t[j+1];

            if (name=="Hash") g_hash_mb = std::stoi(value);
            else if (name=="Threads") g_threads = std::stoi(value);
            else if (name=="UseNNUE") g_use_nnue = (value=="true"||value=="1");
            else if (name=="EvalFile") g_eval_file = value;
            break;
        }
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

    auto best = g_search.find_bestmove(g_board, lim);
    std::string best_uci = g_board.move_to_uci(best);

    std::cout << "info depth 1 score cp 0 nodes 1 time 0 pv" << "\n";
    if (best == MOVE_NONE) {
        std::cout << "bestmove (none)\n";
    } else {
        std::cout << "bestmove " << best_uci << "\n";
    }
    std::cout << std::flush;
}

void Uci::cmd_stop() { g_stop = true; }

} // namespace engine
