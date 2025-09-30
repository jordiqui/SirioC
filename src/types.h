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
    Bitboard bitboards[PIECE_TYPE_NB * COLOR_NB];
    enum Color side_to_move;
    int castling_rights;
    Square en_passant_square;
    int halfmove_clock;
    int fullmove_number;
} Board;

typedef struct HistoryTable {
    int32_t history[COLOR_NB][PIECE_TYPE_NB][64];
} HistoryTable;

typedef struct SearchLimits {
    int depth;
    int movetime_ms;
    int nodes;
    int infinite;
    int multipv;
} SearchLimits;

typedef struct SearchContext {
    Board* board;
    HistoryTable* history;
    SearchLimits limits;
    Value best_value;
    Move best_move;
    Move pv_moves[256];
    Value pv_values[256];
    size_t pv_count;
    int multipv;
    uint64_t start_time_ms;
    int stop;
    int move_overhead;
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

#define VALUE_MATE 32000
#define VALUE_INFINITE 31000
#define VALUE_DRAW 0
#define VALUE_NONE 29000

#ifdef __cplusplus
} /* extern "C" */
#endif

