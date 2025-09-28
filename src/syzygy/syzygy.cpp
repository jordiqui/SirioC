#include "engine/syzygy/syzygy.hpp"

#include "engine/core/board.hpp"
#include "tbprobe.h"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <string>

namespace engine::syzygy {

namespace {

std::mutex g_mutex;
std::atomic<bool> g_ready{false};
TBConfig g_active_config{};
std::string g_current_path;

int convert_tb_promotion(int tb_code) {
    switch (tb_code) {
    case TB_PROMOTES_QUEEN: return 4;
    case TB_PROMOTES_ROOK: return 3;
    case TB_PROMOTES_BISHOP: return 2;
    case TB_PROMOTES_KNIGHT: return 1;
    default: return 0;
    }
}

Move build_move_from_tb(const Board& board, int from, int to, int promo_code, bool is_ep) {
    if (from < 0 || from >= 64 || to < 0 || to >= 64) return MOVE_NONE;

    const auto& squares = board.squares();
    char moving = squares[static_cast<size_t>(from)];
    if (moving == '.') return MOVE_NONE;

    bool capture = board.piece_on(to) != '.' || is_ep;
    bool pawn = std::tolower(static_cast<unsigned char>(moving)) == 'p';
    bool double_push = pawn && std::abs(to - from) == 16;
    int promo = convert_tb_promotion(promo_code);
    return make_move(from, to, promo, capture, double_push, is_ep, false);
}

bool has_en_passant_capture(const Board& board, int ep_square) {
    if (ep_square == Board::INVALID_SQUARE) return false;

    int file = ep_square % 8;
    int rank = ep_square / 8;
    const auto& squares = board.squares();

    if (board.white_to_move()) {
        if (rank != 5) return false;
        int from_rank = rank - 1;
        if (from_rank < 0) return false;
        for (int df : {-1, 1}) {
            int from_file = file + df;
            if (from_file < 0 || from_file >= 8) continue;
            int from_sq = from_rank * 8 + from_file;
            if (squares[static_cast<size_t>(from_sq)] == 'P') return true;
        }
    } else {
        if (rank != 2) return false;
        int from_rank = rank + 1;
        if (from_rank >= 8) return false;
        for (int df : {-1, 1}) {
            int from_file = file + df;
            if (from_file < 0 || from_file >= 8) continue;
            int from_sq = from_rank * 8 + from_file;
            if (squares[static_cast<size_t>(from_sq)] == 'p') return true;
        }
    }
    return false;
}

bool ensure_initialized_locked(const TBConfig& config) {
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
        bool available = TB_LARGEST > 0;
        g_ready.store(available, std::memory_order_release);
        return available;
    }

    g_current_path.clear();
    g_ready.store(false, std::memory_order_release);
    return false;
}

} // namespace

bool configure(const TBConfig& config) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_active_config = config;
    return ensure_initialized_locked(g_active_config);
}

void shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_active_config = TBConfig{};
    if (g_ready.exchange(false, std::memory_order_acq_rel)) {
        tb_free();
    }
    g_current_path.clear();
}

bool is_available() {
    return g_ready.load(std::memory_order_acquire) && TB_LARGEST > 0;
}

namespace TB {

int pieceCount(const Board& board) {
    const auto& occupancy = board.occupancy();
    return static_cast<int>(std::popcount(occupancy[Board::OCC_BOTH]));
}

std::optional<ProbeResult> probePosition(const Board& board, const TBConfig& config,
                                         bool root_probe) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (!config.enabled || config.path.empty()) return std::nullopt;

    if (config.enabled != g_active_config.enabled || config.path != g_active_config.path) {
        g_active_config = config;
        if (!ensure_initialized_locked(g_active_config)) return std::nullopt;
    }

    if (!is_available()) return std::nullopt;
    if (board.castling_rights() != 0) return std::nullopt;

    int pieces = pieceCount(board);
    if (pieces > static_cast<int>(TB_LARGEST)) return std::nullopt;
    if (config.probe_limit >= 0 && pieces > config.probe_limit) return std::nullopt;

    unsigned ep = 0;
    int ep_square = board.en_passant_square();
    if (ep_square != Board::INVALID_SQUARE && has_en_passant_capture(board, ep_square)) {
        ep = static_cast<unsigned>(ep_square);
    }

    const auto& occupancy = board.occupancy();
    const auto& pieces_bb = board.piece_bitboards();

    uint64_t white = occupancy[Board::OCC_WHITE];
    uint64_t black = occupancy[Board::OCC_BLACK];
    uint64_t kings = pieces_bb[Board::WHITE_KING] | pieces_bb[Board::BLACK_KING];
    uint64_t queens = pieces_bb[Board::WHITE_QUEEN] | pieces_bb[Board::BLACK_QUEEN];
    uint64_t rooks = pieces_bb[Board::WHITE_ROOK] | pieces_bb[Board::BLACK_ROOK];
    uint64_t bishops = pieces_bb[Board::WHITE_BISHOP] | pieces_bb[Board::BLACK_BISHOP];
    uint64_t knights = pieces_bb[Board::WHITE_KNIGHT] | pieces_bb[Board::BLACK_KNIGHT];
    uint64_t pawns = pieces_bb[Board::WHITE_PAWN] | pieces_bb[Board::BLACK_PAWN];
    bool turn = board.white_to_move();

    unsigned rule50 = config.use_rule50 ? static_cast<unsigned>(board.halfmove_clock()) : 0;
    unsigned castling = 0; // already filtered positions with castling rights

    if (root_probe) {
        unsigned result = tb_probe_root(white, black, kings, queens, rooks, bishops, knights,
                                        pawns, rule50, castling, ep, turn, nullptr);
        if (result == TB_RESULT_FAILED) return std::nullopt;

        ProbeResult out;
        out.wdl = static_cast<WdlOutcome>(TB_GET_WDL(result));

        int dtz = static_cast<int>(TB_GET_DTZ(result));
        if (dtz != 0 && (out.wdl == WdlOutcome::Loss || out.wdl == WdlOutcome::BlessedLoss)) {
            dtz = -dtz;
        }
        if (dtz != 0) out.dtz = dtz;

        int from = TB_GET_FROM(result);
        int to = TB_GET_TO(result);
        int promo = TB_GET_PROMOTES(result);
        bool is_ep = TB_GET_EP(result) != 0;
        Move move = build_move_from_tb(board, from, to, promo, is_ep);
        if (move != MOVE_NONE) out.best_move = move;
        return out;
    }

    unsigned wdl = tb_probe_wdl(white, black, kings, queens, rooks, bishops, knights, pawns,
                                rule50, castling, ep, turn);
    if (wdl == TB_RESULT_FAILED) return std::nullopt;

    ProbeResult out;
    out.wdl = static_cast<WdlOutcome>(wdl);
    return out;
}

} // namespace TB

} // namespace engine::syzygy
