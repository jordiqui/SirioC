#include "engine/syzygy/syzygy.hpp"

#include "engine/core/board.hpp"
#include "engine/syzygy/tbprobe.h"

#include <atomic>
#include <bit>
#include <mutex>
#include <string>

namespace engine::syzygy {

namespace {

std::mutex g_mutex;
std::atomic<bool> g_ready{false};
std::string g_current_path;

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

} // namespace

bool init(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (path.empty()) {
        if (g_ready.exchange(false, std::memory_order_acq_rel)) {
            tb_free();
        }
        g_current_path.clear();
        return false;
    }

    if (g_ready.load(std::memory_order_acquire) && path == g_current_path) {
        return TB_LARGEST > 0;
    }

    if (g_ready.exchange(false, std::memory_order_acq_rel)) {
        tb_free();
    }

    if (tb_init(path.c_str())) {
        g_current_path = path;
        bool available = TB_LARGEST > 0;
        g_ready.store(available, std::memory_order_release);
        return available;
    }

    g_current_path = path;
    g_ready.store(false, std::memory_order_release);
    return false;
}

void shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_ready.exchange(false, std::memory_order_acq_rel)) {
        tb_free();
    }
    g_current_path.clear();
}

bool is_available() {
    return g_ready.load(std::memory_order_acquire) && TB_LARGEST > 0;
}

std::optional<ProbeResult> probe_wdl(const Board& board) {
    if (!is_available()) return std::nullopt;

    const auto& occupancy = board.occupancy();
    uint64_t all = occupancy[Board::OCC_BOTH];
    if (std::popcount(all) > TB_LARGEST) return std::nullopt;

    if (board.castling_rights() != 0) return std::nullopt;

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

    unsigned result = tb_probe_wdl_impl(
        white,
        black,
        kings,
        queens,
        rooks,
        bishops,
        knights,
        pawns,
        ep,
        board.white_to_move());

    if (result == TB_RESULT_FAILED) return std::nullopt;

    ProbeResult out;
    out.wdl = static_cast<WdlOutcome>(result);
    return out;
}

} // namespace engine::syzygy

