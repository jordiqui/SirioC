#include "tb.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "bits.h"
#include "move.h"
#include <tbprobe.h>

static char g_syzygy_path[1024] = "";
static int g_syzygy_probe_depth = 1;
static int g_syzygy_50_move_rule = 1;
static int g_syzygy_probe_limit = 6;
static int g_tb_ready = 0;
static unsigned g_tb_largest = 0;

static unsigned effective_probe_limit(void) {
    if (g_syzygy_probe_limit <= 0) {
        return 0U;
    }
    unsigned limit = (unsigned)g_syzygy_probe_limit;
    if (g_tb_largest != 0 && limit > g_tb_largest) {
        limit = g_tb_largest;
    }
    return limit;
}

static int count_pieces(const Board* board) {
    int total = 0;
    for (int color = 0; color < COLOR_NB; ++color) {
        for (int piece = PIECE_PAWN; piece < PIECE_TYPE_NB; ++piece) {
            total += bits_popcount(board->bitboards[piece + PIECE_TYPE_NB * color]);
        }
    }
    return total;
}

static uint64_t piece_bitboard(const Board* board, Piece piece) {
    return board->bitboards[piece + PIECE_TYPE_NB * COLOR_WHITE] |
           board->bitboards[piece + PIECE_TYPE_NB * COLOR_BLACK];
}

static unsigned board_ep_square(const Board* board) {
    if (!board || board->en_passant_square < 0) {
        return 0U;
    }
    return (unsigned)board->en_passant_square;
}

static unsigned adjust_wdl(unsigned wdl) {
    if (!g_syzygy_50_move_rule) {
        if (wdl == TB_CURSED_WIN) {
            return TB_WIN;
        }
        if (wdl == TB_BLESSED_LOSS) {
            return TB_LOSS;
        }
    }
    return wdl;
}

static Value wdl_to_value(unsigned wdl) {
    wdl = adjust_wdl(wdl);
    switch (wdl) {
        case TB_WIN:
            return VALUE_MATE - 1;
        case TB_CURSED_WIN:
            return g_syzygy_50_move_rule ? 1 : VALUE_MATE - 1;
        case TB_DRAW:
            return VALUE_DRAW;
        case TB_BLESSED_LOSS:
            return g_syzygy_50_move_rule ? -1 : -VALUE_MATE + 1;
        case TB_LOSS:
            return -VALUE_MATE + 1;
        default:
            return VALUE_NONE;
    }
}

static bool tb_load_tables(void) {
    if (!tb_init(g_syzygy_path)) {
        g_tb_ready = 0;
        g_tb_largest = 0;
        return false;
    }
    g_tb_largest = TB_LARGEST;
    g_tb_ready = g_tb_largest > 0;
    return g_tb_ready;
}

static bool tb_can_probe(const Board* board) {
    if (!g_tb_ready || !board) {
        return false;
    }
    unsigned limit = effective_probe_limit();
    if (limit == 0) {
        return false;
    }
    if (count_pieces(board) > (int)limit) {
        return false;
    }
    return true;
}

void tb_initialize(void) {
    tb_load_tables();
}

void tb_set_path(const char* path) {
    if (!path) {
        g_syzygy_path[0] = '\0';
        g_tb_ready = 0;
        g_tb_largest = 0;
        tb_free();
        return;
    }
    size_t len = strlen(path);
    if (len >= sizeof(g_syzygy_path)) {
        len = sizeof(g_syzygy_path) - 1;
    }
    memcpy(g_syzygy_path, path, len);
    g_syzygy_path[len] = '\0';
    tb_free();
    tb_load_tables();
}

const char* tb_get_path(void) {
    return g_syzygy_path;
}

void tb_set_probe_depth(int depth) {
    if (depth < 0) {
        depth = 0;
    }
    g_syzygy_probe_depth = depth;
}

int tb_get_probe_depth(void) {
    return g_syzygy_probe_depth;
}

void tb_set_50_move_rule(int enabled) {
    g_syzygy_50_move_rule = enabled ? 1 : 0;
}

int tb_get_50_move_rule(void) {
    return g_syzygy_50_move_rule;
}

void tb_set_probe_limit(int limit) {
    if (limit < 0) {
        limit = 0;
    }
    g_syzygy_probe_limit = limit;
}

int tb_get_probe_limit(void) {
    return g_syzygy_probe_limit;
}

Value tb_probe(const Board* board) {
    if (!tb_can_probe(board)) {
        return VALUE_NONE;
    }

    uint64_t white = board_occupancy(board, COLOR_WHITE);
    uint64_t black = board_occupancy(board, COLOR_BLACK);
    uint64_t kings = piece_bitboard(board, PIECE_KING);
    uint64_t queens = piece_bitboard(board, PIECE_QUEEN);
    uint64_t rooks = piece_bitboard(board, PIECE_ROOK);
    uint64_t bishops = piece_bitboard(board, PIECE_BISHOP);
    uint64_t knights = piece_bitboard(board, PIECE_KNIGHT);
    uint64_t pawns = piece_bitboard(board, PIECE_PAWN);

    unsigned ep = board_ep_square(board);
    bool turn = board->side_to_move == COLOR_WHITE;

    unsigned wdl = tb_probe_wdl(white,
                                black,
                                kings,
                                queens,
                                rooks,
                                bishops,
                                knights,
                                pawns,
                                0,
                                0,
                                ep,
                                turn);

    if (wdl == TB_RESULT_FAILED) {
        return VALUE_NONE;
    }

    return wdl_to_value(wdl);
}

static Piece promotion_from_tb(unsigned promote_code) {
    switch (promote_code) {
        case TB_PROMOTES_QUEEN:
            return PIECE_QUEEN;
        case TB_PROMOTES_ROOK:
            return PIECE_ROOK;
        case TB_PROMOTES_BISHOP:
            return PIECE_BISHOP;
        case TB_PROMOTES_KNIGHT:
            return PIECE_KNIGHT;
        default:
            return PIECE_NONE;
    }
}

int tb_probe_root_position(const Board* board, Move* out_move, Value* out_value) {
    if (!out_move || !out_value || !tb_can_probe(board)) {
        return 0;
    }

    uint64_t white = board_occupancy(board, COLOR_WHITE);
    uint64_t black = board_occupancy(board, COLOR_BLACK);
    uint64_t kings = piece_bitboard(board, PIECE_KING);
    uint64_t queens = piece_bitboard(board, PIECE_QUEEN);
    uint64_t rooks = piece_bitboard(board, PIECE_ROOK);
    uint64_t bishops = piece_bitboard(board, PIECE_BISHOP);
    uint64_t knights = piece_bitboard(board, PIECE_KNIGHT);
    uint64_t pawns = piece_bitboard(board, PIECE_PAWN);

    unsigned ep = board_ep_square(board);
    unsigned rule50 = g_syzygy_50_move_rule ? (unsigned)board->halfmove_clock : 0U;
    bool turn = board->side_to_move == COLOR_WHITE;

    unsigned result = tb_probe_root_impl(white,
                                         black,
                                         kings,
                                         queens,
                                         rooks,
                                         bishops,
                                         knights,
                                         pawns,
                                         rule50,
                                         ep,
                                         turn,
                                         NULL);

    if (result == TB_RESULT_FAILED) {
        return 0;
    }

    if (result == TB_RESULT_STALEMATE) {
        *out_move = move_create(0, 0, PIECE_NONE, PIECE_NONE, PIECE_NONE, 0);
        *out_value = VALUE_DRAW;
        return 1;
    }

    if (result == TB_RESULT_CHECKMATE) {
        *out_move = move_create(0, 0, PIECE_NONE, PIECE_NONE, PIECE_NONE, 0);
        *out_value = -VALUE_MATE + 1;
        return 1;
    }

    Square from = (Square)TB_GET_FROM(result);
    Square to = (Square)TB_GET_TO(result);
    if (from < 0 || from >= 64 || to < 0 || to >= 64) {
        return 0;
    }

    Piece piece = board->squares[from];
    if (piece == PIECE_NONE) {
        return 0;
    }
    Piece capture = board->squares[to];
    if (capture == PIECE_NONE && TB_GET_EP(result)) {
        capture = PIECE_PAWN;
    }
    Piece promotion = promotion_from_tb(TB_GET_PROMOTES(result));

    *out_move = move_create(from, to, piece, capture, promotion, 0);
    *out_value = wdl_to_value(TB_GET_WDL(result));
    return (*out_value != VALUE_NONE);
}

