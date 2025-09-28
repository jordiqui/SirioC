#include "engine/syzygy/syzygy.hpp"

#include "engine/core/board.hpp"
#include "engine/syzygy/tbprobe.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine::syzygy {

namespace {

std::mutex g_mutex;
std::atomic<bool> g_ready{false};
TBConfig g_config{};
std::string g_current_path;

bool has_tablebases_loaded() {
    return g_ready.load(std::memory_order_acquire) && TB_LARGEST > 0;
}

unsigned to_tb_move_bits(unsigned from, unsigned to, unsigned promotes) {
    unsigned value = 0;
    value = TB_SET_FROM(value, from);
    value = TB_SET_TO(value, to);
    value = TB_SET_PROMOTES(value, promotes);
    return value;
}

} // namespace

bool TB::init(const TBConfig& config) {
    std::lock_guard<std::mutex> lock(g_mutex);

    g_config = config;

    if (!config.enabled || config.path.empty()) {
        if (g_ready.exchange(false, std::memory_order_acq_rel)) {
            tb_free();
        }
        g_current_path.clear();
        return false;
    }

    if (g_ready.load(std::memory_order_acquire) && config.path == g_current_path) {
        return TB_LARGEST > 0;
    }

    if (g_ready.exchange(false, std::memory_order_acq_rel)) {
        tb_free();
    }

    if (tb_init(config.path.c_str())) {
        g_current_path = config.path;
        bool ready = TB_LARGEST > 0;
        g_ready.store(ready, std::memory_order_release);
        return ready;
    }

    g_current_path = config.path;
    g_ready.store(false, std::memory_order_release);
    return false;
}

void TB::release() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_ready.exchange(false, std::memory_order_acq_rel)) {
        tb_free();
    }
    g_current_path.clear();
}

int TB::pieceCount() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!has_tablebases_loaded()) return 0;
    return static_cast<int>(TB_LARGEST);
}

std::optional<TBResult> TB::probePosition(const Board& board, TBProbe probeType,
                                          int searchDepth) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (!g_config.enabled || !has_tablebases_loaded()) return std::nullopt;

    int pieces = board.countPiecesTotal();
    if (pieces > static_cast<int>(TB_LARGEST)) return std::nullopt;
    if (g_config.probeLimit > 0 && pieces > g_config.probeLimit) return std::nullopt;

    if (!any(probeType, TBProbe::Root) && searchDepth < g_config.probeDepth) {
        return std::nullopt;
    }

    auto state = board.exportToFathom();
    if (state.castling != 0) return std::nullopt;

    unsigned rule50 = g_config.useRule50 ? state.rule50 : 0U;

    TBResult result;

    bool attemptRoot = any(probeType, TBProbe::Root) && board.plyFromRoot() == 0;
    if (attemptRoot) {
        unsigned results[TB_MAX_MOVES] = {};
        unsigned root = tb_probe_root(state.white, state.black, state.kings, state.queens,
                                      state.rooks, state.bishops, state.knights, state.pawns,
                                      rule50, state.castling, state.ep, state.whiteToMove,
                                      results);

        if (root != TB_RESULT_FAILED) {
            result.probe = TBProbe::Root;
            result.wdl = static_cast<int>(TB_GET_WDL(root));
            result.dtz = static_cast<int>(TB_GET_DTZ(root));

            unsigned tb_move_bits = to_tb_move_bits(TB_GET_FROM(root), TB_GET_TO(root),
                                                    TB_GET_PROMOTES(root));
            bool ep_hint = TB_GET_EP(root) != 0;
            result.bestMove = board.convertFromTbMove(static_cast<TbMove>(tb_move_bits), ep_hint);

            std::unordered_map<Move, std::pair<int, int>> wdl_dtz_map;
            std::vector<Move> move_order;
            for (unsigned i = 0; i < TB_MAX_MOVES && results[i] != TB_RESULT_FAILED; ++i) {
                unsigned res = results[i];
                unsigned move_bits = to_tb_move_bits(TB_GET_FROM(res), TB_GET_TO(res),
                                                     TB_GET_PROMOTES(res));
                bool ep = TB_GET_EP(res) != 0;
                Move move = board.convertFromTbMove(static_cast<TbMove>(move_bits), ep);
                if (move == MOVE_NONE) continue;
                move_order.push_back(move);
                int wdl = static_cast<int>(TB_GET_WDL(res));
                int dtz = static_cast<int>(TB_GET_DTZ(res));
                wdl_dtz_map[move] = {wdl, dtz};
            }

            TbRootMoves tb_moves{};
            bool have_scores = tb_probe_root_dtz(state.white, state.black, state.kings,
                                                 state.queens, state.rooks, state.bishops,
                                                 state.knights, state.pawns, rule50,
                                                 state.castling, state.ep, state.whiteToMove,
                                                 false, g_config.useRule50, &tb_moves) != 0;
            if (!have_scores) {
                have_scores = tb_probe_root_wdl(state.white, state.black, state.kings,
                                                state.queens, state.rooks, state.bishops,
                                                state.knights, state.pawns, rule50,
                                                state.castling, state.ep, state.whiteToMove,
                                                g_config.useRule50, &tb_moves) != 0;
            }

            std::unordered_map<Move, std::pair<int, int>> score_rank_map;
            if (have_scores) {
                for (unsigned i = 0; i < tb_moves.size; ++i) {
                    const auto& entry = tb_moves.moves[i];
                    Move move = board.convertFromTbMove(entry.move);
                    if (move == MOVE_NONE) continue;
                    score_rank_map[move] = {entry.tbScore, entry.tbRank};
                }
            }

            for (Move move : move_order) {
                TBMoveInfo info;
                info.move = move;
                if (auto it = wdl_dtz_map.find(move); it != wdl_dtz_map.end()) {
                    info.wdl = it->second.first;
                    info.dtz = it->second.second;
                }
                if (auto it = score_rank_map.find(move); it != score_rank_map.end()) {
                    info.score = it->second.first;
                    info.rank = it->second.second;
                }
                result.moves.push_back(info);
            }

            for (const auto& [move, score_rank] : score_rank_map) {
                auto exists = std::find_if(result.moves.begin(), result.moves.end(),
                                           [&](const TBMoveInfo& info) {
                                               return info.move == move;
                                           });
                if (exists != result.moves.end()) continue;
                TBMoveInfo info;
                info.move = move;
                info.score = score_rank.first;
                info.rank = score_rank.second;
                if (auto it = wdl_dtz_map.find(move); it != wdl_dtz_map.end()) {
                    info.wdl = it->second.first;
                    info.dtz = it->second.second;
                }
                result.moves.push_back(info);
            }

            if (result.moves.empty() && result.bestMove != MOVE_NONE) {
                TBMoveInfo info;
                info.move = result.bestMove;
                if (auto it = wdl_dtz_map.find(result.bestMove); it != wdl_dtz_map.end()) {
                    info.wdl = it->second.first;
                    info.dtz = it->second.second;
                }
                if (auto it = score_rank_map.find(result.bestMove); it != score_rank_map.end()) {
                    info.score = it->second.first;
                    info.rank = it->second.second;
                }
                result.moves.push_back(info);
            }

            if (result.bestMove != MOVE_NONE) {
                if (auto it = score_rank_map.find(result.bestMove); it != score_rank_map.end()) {
                    result.tbScore = it->second.first;
                }
            }

            return result;
        }
    }

    if (any(probeType, TBProbe::Wdl)) {
        unsigned wdl = tb_probe_wdl(state.white, state.black, state.kings, state.queens,
                                    state.rooks, state.bishops, state.knights, state.pawns,
                                    rule50, state.castling, state.ep, state.whiteToMove);
        if (wdl != TB_RESULT_FAILED) {
            result.probe = TBProbe::Wdl;
            result.wdl = static_cast<int>(wdl);
            return result;
        }
    }

    return std::nullopt;
}

} // namespace engine::syzygy
