#include "sirio/syzygy.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <mutex>
#include <optional>
#include <string>

#define TB_NO_THREADS
#define TB_NO_HELPER_API
extern "C" {
#include "tbprobe.h"
}

#include "sirio/move.hpp"

namespace sirio::syzygy {

namespace {
std::string g_tb_path;
bool g_initialized = false;
std::mutex g_mutex;

bool has_castling_rights(const Board &board) {
    const auto &rights = board.castling_rights();
    return rights.white_kingside || rights.white_queenside || rights.black_kingside ||
           rights.black_queenside;
}

int total_pieces(const Board &board) {
    int total = 0;
    for (int color_index = 0; color_index < 2; ++color_index) {
        Color color = color_index == 0 ? Color::White : Color::Black;
        for (int pt = 0; pt < static_cast<int>(PieceType::Count); ++pt) {
            auto type = static_cast<PieceType>(pt);
            total += std::popcount(board.pieces(color, type));
        }
    }
    return total;
}

std::optional<Move> move_from_result(const Board &board, unsigned result) {
    if (result == TB_RESULT_FAILED) {
        return std::nullopt;
    }
    int from = TB_GET_FROM(result);
    int to = TB_GET_TO(result);
    auto from_piece = board.piece_at(from);
    if (!from_piece.has_value()) {
        return std::nullopt;
    }
    Move move{from, to, from_piece->second};
    int promotes = TB_GET_PROMOTES(result);
    switch (promotes) {
        case TB_PROMOTES_NONE:
            break;
        case TB_PROMOTES_QUEEN:
            move.promotion = PieceType::Queen;
            break;
        case TB_PROMOTES_ROOK:
            move.promotion = PieceType::Rook;
            break;
        case TB_PROMOTES_BISHOP:
            move.promotion = PieceType::Bishop;
            break;
        case TB_PROMOTES_KNIGHT:
            move.promotion = PieceType::Knight;
            break;
        default:
            break;
    }

    if (TB_GET_EP(result) != 0) {
        move.is_en_passant = true;
        move.captured = PieceType::Pawn;
    } else {
        auto captured = board.piece_at(to);
        if (captured && captured->first != from_piece->first) {
            move.captured = captured->second;
        }
    }
    return move;
}

bool ensure_initialized_locked() {
    if (g_initialized) {
        return true;
    }
    if (g_tb_path.empty()) {
        return false;
    }
    g_initialized = tb_init(g_tb_path.c_str());
    return g_initialized;
}

unsigned encode_ep(const Board &board) {
    auto ep = board.en_passant_square();
    if (!ep) {
        return 0;
    }
    return static_cast<unsigned>(*ep);
}

}  // namespace

void set_tablebase_path(const std::string &path) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_tb_path = path;
    g_initialized = false;
    if (!g_tb_path.empty()) {
        g_initialized = tb_init(g_tb_path.c_str());
    }
}

const std::string &tablebase_path() { return g_tb_path; }

bool available() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return ensure_initialized_locked();
}

int max_pieces() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!ensure_initialized_locked()) {
        return 0;
    }
    return static_cast<int>(TB_LARGEST);
}

std::optional<ProbeResult> probe_wdl(const Board &board) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!ensure_initialized_locked()) {
        return std::nullopt;
    }
    if (has_castling_rights(board)) {
        return std::nullopt;
    }
    if (total_pieces(board) > static_cast<int>(TB_LARGEST)) {
        return std::nullopt;
    }

    unsigned result = tb_probe_wdl(board.occupancy(Color::White), board.occupancy(Color::Black),
                                   board.pieces(Color::White, PieceType::King) |
                                       board.pieces(Color::Black, PieceType::King),
                                   board.pieces(Color::White, PieceType::Queen) |
                                       board.pieces(Color::Black, PieceType::Queen),
                                   board.pieces(Color::White, PieceType::Rook) |
                                       board.pieces(Color::Black, PieceType::Rook),
                                   board.pieces(Color::White, PieceType::Bishop) |
                                       board.pieces(Color::Black, PieceType::Bishop),
                                   board.pieces(Color::White, PieceType::Knight) |
                                       board.pieces(Color::Black, PieceType::Knight),
                                   board.pieces(Color::White, PieceType::Pawn) |
                                       board.pieces(Color::Black, PieceType::Pawn),
                                   0, 0, encode_ep(board), board.side_to_move() == Color::White);
    if (result == TB_RESULT_FAILED) {
        return std::nullopt;
    }
    ProbeResult output;
    output.wdl = static_cast<int>(result) - 2;
    output.dtz = 0;
    return output;
}

std::optional<ProbeResult> probe_root(const Board &board) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!ensure_initialized_locked()) {
        return std::nullopt;
    }
    if (has_castling_rights(board)) {
        return std::nullopt;
    }
    if (total_pieces(board) > static_cast<int>(TB_LARGEST)) {
        return std::nullopt;
    }

    unsigned result = tb_probe_root(board.occupancy(Color::White), board.occupancy(Color::Black),
                                    board.pieces(Color::White, PieceType::King) |
                                        board.pieces(Color::Black, PieceType::King),
                                    board.pieces(Color::White, PieceType::Queen) |
                                        board.pieces(Color::Black, PieceType::Queen),
                                    board.pieces(Color::White, PieceType::Rook) |
                                        board.pieces(Color::Black, PieceType::Rook),
                                    board.pieces(Color::White, PieceType::Bishop) |
                                        board.pieces(Color::Black, PieceType::Bishop),
                                    board.pieces(Color::White, PieceType::Knight) |
                                        board.pieces(Color::Black, PieceType::Knight),
                                    board.pieces(Color::White, PieceType::Pawn) |
                                        board.pieces(Color::Black, PieceType::Pawn),
                                    static_cast<unsigned>(board.halfmove_clock()), 0, encode_ep(board),
                                    board.side_to_move() == Color::White, nullptr);
    if (result == TB_RESULT_FAILED) {
        return std::nullopt;
    }

    ProbeResult output;
    output.wdl = static_cast<int>(TB_GET_WDL(result)) - 2;
    output.dtz = static_cast<int>(TB_GET_DTZ(result));
    output.best_move = move_from_result(board, result);
    return output;
}

}  // namespace sirio::syzygy

