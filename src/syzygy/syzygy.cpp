#include "engine/syzygy/syzygy.hpp"

#include "engine/core/board.hpp"
#include "tbprobe.h"

#include <atomic>
#include <bit>
#include <cctype>
#include <cstdlib>
#include <mutex>
#include <string>

namespace engine::syzygy {

namespace {

std::mutex g_mutex;
std::atomic<bool> g_ready{false};
std::string g_current_path;
TBConfig g_active_config{};

constexpr uint64_t kFileA = 0x0101010101010101ULL;
constexpr uint64_t kFileH = 0x8080808080808080ULL;

bool has_en_passant_capture(const Board& board, int ep_square) {
    if (ep_square == Board::INVALID_SQUARE) return false;
    const auto& pieces = board.piece_bitboards();
    uint64_t ep_mask = 1ULL << ep_square;
    if (board.white_to_move()) {
        uint64_t pawns = pieces[Board::WHITE_PAWN];
        uint64_t left = (ep_mask & ~kFileH) >> 7;
        uint64_t right = (ep_mask & ~kFileA) >> 9;
        return (pawns & (left | right)) != 0;
    } else {
        uint64_t pawns = pieces[Board::BLACK_PAWN];
        uint64_t left = (ep_mask & ~kFileA) << 7;
        uint64_t right = (ep_mask & ~kFileH) << 9;
        return (pawns & (left | right)) != 0;
    }
}

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
    const auto& squares = board.squares();
    char moving = squares[static_cast<size_t>(from)];
    bool capture = board.piece_on(to) != '.' || is_ep;
    bool pawn = std::tolower(static_cast<unsigned char>(moving)) == 'p';
    bool double_push = pawn && std::abs(to - from) == 16;
    int promo = convert_tb_promotion(promo_code);
    return make_move(from, to, promo, capture, double_push, is_ep, false);
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

    g_current_path = config.path;
    g_ready.store(false, std::memory_order_release);
    return false;
}

} // namespace

bool configure(const TBConfig& config) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_active_config = config;
    return ensure_initialized_locked(config);
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
    return g_active_config.enabled &&
           g_ready.load(std::memory_order_acquire) && TB_LARGEST > 0;
}

namespace TB {

int pieceCount(const Board& board) {
    const auto& occupancy = board.occupancy();
    return static_cast<int>(std::popcount(occupancy[Board::OCC_BOTH]));
}

std::optional<ProbeResult> probePosition(const Board& board, const TBConfig& config,
                                         bool root) {
    if (!config.enabled || config.path.empty()) return std::nullopt;
    if (!is_available()) return std::nullopt;
    if (board.castling_rights() != 0) return std::nullopt;

    const auto& occupancy = board.occupancy();
    if (static_cast<int>(std::popcount(occupancy[Board::OCC_BOTH])) > TB_LARGEST) {
        return std::nullopt;
    }

    unsigned ep = 0;
    int ep_square = board.en_passant_square();
    if (ep_square != Board::INVALID_SQUARE && has_en_passant_capture(board, ep_square)) {
        ep = static_cast<unsigned>(ep_square);
    }

    const auto& pieces = board.piece_bitboards();
    uint64_t white = occupancy[Board::OCC_WHITE];
    uint64_t black = occupancy[Board::OCC_BLACK];
    uint64_t kings = pieces[Board::WHITE_KING] | pieces[Board::BLACK_KING];
    uint64_t queens = pieces[Board::WHITE_QUEEN] | pieces[Board::BLACK_QUEEN];
    uint64_t rooks = pieces[Board::WHITE_ROOK] | pieces[Board::BLACK_ROOK];
    uint64_t bishops = pieces[Board::WHITE_BISHOP] | pieces[Board::BLACK_BISHOP];
    uint64_t knights = pieces[Board::WHITE_KNIGHT] | pieces[Board::BLACK_KNIGHT];
    uint64_t pawns = pieces[Board::WHITE_PAWN] | pieces[Board::BLACK_PAWN];
    bool turn = board.white_to_move();

    if (root) {
        unsigned result = tb_probe_root_impl(white, black, kings, queens, rooks, bishops,
                                             knights, pawns,
                                             static_cast<unsigned>(board.halfmove_clock()),
                                             ep, turn, nullptr);
        if (result == TB_RESULT_FAILED) return std::nullopt;

        ProbeResult out;
        if (result == TB_RESULT_CHECKMATE) {
            out.wdl = WdlOutcome::Loss;
            out.dtz = -1;
            return out;
        }
        if (result == TB_RESULT_STALEMATE) {
            out.wdl = WdlOutcome::Draw;
            out.dtz = 0;
            return out;
        }

        out.wdl = static_cast<WdlOutcome>(TB_GET_WDL(result));
        int dtz = static_cast<int>(TB_GET_DTZ(result));
        if (dtz != 0 &&
            (out.wdl == WdlOutcome::Loss || out.wdl == WdlOutcome::BlessedLoss)) {
            dtz = -dtz;
        }
        out.dtz = dtz;

        int from = TB_GET_FROM(result);
        int to = TB_GET_TO(result);
        int promo = TB_GET_PROMOTES(result);
        bool is_ep = TB_GET_EP(result) != 0;
        out.best_move = build_move_from_tb(board, from, to, promo, is_ep);
        return out;
    }

    unsigned result = tb_probe_wdl_impl(white, black, kings, queens, rooks, bishops,
                                        knights, pawns, ep, turn);
    if (result == TB_RESULT_FAILED) return std::nullopt;

    ProbeResult out;
    out.wdl = static_cast<WdlOutcome>(result);
    return out;
}

} // namespace TB

} // namespace engine::syzygy

