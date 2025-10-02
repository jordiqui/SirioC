#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t Bitboard;
typedef uint32_t MoveKey;
typedef int Square;
typedef int Piece;
typedef int Value;

enum Color {
    COLOR_WHITE = 0,
    COLOR_BLACK = 1,
    COLOR_NB
};

enum PieceType {
    PIECE_NONE = 0,
    PIECE_PAWN,
    PIECE_KNIGHT,
    PIECE_BISHOP,
    PIECE_ROOK,
    PIECE_QUEEN,
    PIECE_KING,
    PIECE_TYPE_NB
};

typedef struct Move {
    Square from;
    Square to;
    Piece piece;
    Piece capture;
    Piece promotion;
    int flags;
} Move;

typedef struct MoveList {
    Move moves[256];
    size_t size;
} MoveList;

typedef struct Board {
    Piece squares[64];
    Bitboard bitboards[((size_t)PIECE_TYPE_NB) * ((size_t)COLOR_NB)];
    enum Color side_to_move;
    int castling_rights;
    Square en_passant_square;
    int halfmove_clock;
    int fullmove_number;
    uint64_t zobrist_key;
} Board;

typedef struct HistoryTable {
    int32_t history[COLOR_NB][PIECE_TYPE_NB][64];
} HistoryTable;

typedef struct TranspositionTable TranspositionTable;

typedef struct SearchLimits {
    int depth;
    int movetime_ms;
    int nodes;
    int infinite;
    int multipv;
    int wtime_ms;
    int btime_ms;
    int winc_ms;
    int binc_ms;
    int moves_to_go;
    int ponder;
} SearchLimits;

typedef struct SearchContext {
    Board* board;
    HistoryTable* history;
    TranspositionTable* tt;
    SearchLimits limits;
    Value best_value;
    Move best_move;
    Move pv_moves[256];
    Value pv_values[256];
    size_t pv_count;
    Move pv_table[256][64];
    int pv_lengths[256];
    int multipv;
    uint64_t start_time_ms;
    int stop;
    int move_overhead;
    uint64_t nodes;
    int depth_completed;
    int seldepth;
    uint64_t last_search_time_ms;
    uint64_t last_info_report_ms;
} SearchContext;

typedef struct ThreadContext {
    SearchContext* search;
    int id;
} ThreadContext;

typedef struct TranspositionEntry {
    uint64_t key;
    Value value;
    Move best_move;
    int depth;
    int flags;
} TranspositionEntry;

typedef struct TranspositionTable {
    TranspositionEntry* entries;
    size_t size;
} TranspositionTable;

enum {
    TT_FLAG_EXACT = 0,
    TT_FLAG_LOWER = 1,
    TT_FLAG_UPPER = 2
};

#define VALUE_MATE 32000
#define VALUE_INFINITE 31000
#define VALUE_DRAW 0
#define VALUE_NONE 29000

#ifdef __cplusplus
} /* extern "C" */
#endif

