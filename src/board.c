#include "board.h"
#include "bits.h"
#include "zobrist.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void board_init(Board* board) {
    if (board == NULL) {
        return;
    }
    memset(board, 0, sizeof(*board));
    board->en_passant_square = -1;
    board->side_to_move = COLOR_WHITE;
    board->zobrist_key = 0ULL;
}

static void set_piece(Board* board, Square square, Piece piece, enum Color color) {
    board->squares[square] = piece;
    board->bitboards[piece + PIECE_TYPE_NB * color] |= 1ULL << square;
}

void board_set_start_position(Board* board) {
    board_init(board);
    if (board == NULL) {
        return;
    }

    for (int i = 8; i < 16; ++i) {
        set_piece(board, i, PIECE_PAWN, COLOR_WHITE);
        set_piece(board, 48 + (i - 8), PIECE_PAWN, COLOR_BLACK);
    }

    const Piece white_back_rank[8] = {
        PIECE_ROOK, PIECE_KNIGHT, PIECE_BISHOP, PIECE_QUEEN,
        PIECE_KING, PIECE_BISHOP, PIECE_KNIGHT, PIECE_ROOK
    };

    for (int file = 0; file < 8; ++file) {
        set_piece(board, file, white_back_rank[file], COLOR_WHITE);
        set_piece(board, 56 + file, white_back_rank[file], COLOR_BLACK);
    }

    board->side_to_move = COLOR_WHITE;
    board->castling_rights = 0xF;
    board->en_passant_square = -1;
    board->halfmove_clock = 0;
    board->fullmove_number = 1;
    board->zobrist_key = zobrist_compute_key(board);
}

static Piece piece_from_fen_char(char symbol, enum Color* color_out) {
    *color_out = isupper((unsigned char)symbol) ? COLOR_WHITE : COLOR_BLACK;
    char lower = (char)tolower((unsigned char)symbol);
    switch (lower) {
        case 'p':
            return PIECE_PAWN;
        case 'n':
            return PIECE_KNIGHT;
        case 'b':
            return PIECE_BISHOP;
        case 'r':
            return PIECE_ROOK;
        case 'q':
            return PIECE_QUEEN;
        case 'k':
            return PIECE_KING;
        default:
            return PIECE_NONE;
    }
}

int board_set_fen(Board* board, const char* fen) {
    if (board == NULL || fen == NULL) {
        return 0;
    }

    board_init(board);

    char buffer[512];
    strncpy(buffer, fen, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char* tokens[6] = {0};
    size_t token_count = 0;
    char* save_ptr = NULL;
    char* token = strtok_r(buffer, " ", &save_ptr);
    while (token && token_count < 6) {
        tokens[token_count++] = token;
        token = strtok_r(NULL, " ", &save_ptr);
    }

    if (token_count < 4) {
        return 0;
    }

    int rank = 7;
    int file = 0;
    for (const char* cursor = tokens[0]; *cursor && rank >= 0; ++cursor) {
        char symbol = *cursor;
        if (symbol == '/') {
            --rank;
            file = 0;
            continue;
        }
        if (isdigit((unsigned char)symbol)) {
            file += symbol - '0';
            if (file > 8) {
                return 0;
            }
            continue;
        }

        enum Color piece_color = COLOR_WHITE;
        Piece piece = piece_from_fen_char(symbol, &piece_color);
        if (piece == PIECE_NONE || file >= 8 || rank < 0) {
            return 0;
        }
        Square square = rank * 8 + file;
        set_piece(board, square, piece, piece_color);
        ++file;
    }

    board->side_to_move = (tokens[1][0] == 'b') ? COLOR_BLACK : COLOR_WHITE;

    int castling = 0;
    if (tokens[2][0] != '-') {
        for (const char* cursor = tokens[2]; *cursor; ++cursor) {
            switch (*cursor) {
                case 'K':
                    castling |= 1;
                    break;
                case 'Q':
                    castling |= 2;
                    break;
                case 'k':
                    castling |= 4;
                    break;
                case 'q':
                    castling |= 8;
                    break;
                default:
                    break;
            }
        }
    }
    board->castling_rights = castling;

    board->en_passant_square = -1;
    if (tokens[3][0] != '-') {
        if (strlen(tokens[3]) == 2) {
            int file_index = tokens[3][0] - 'a';
            int rank_index = tokens[3][1] - '1';
            if (file_index >= 0 && file_index < 8 && rank_index >= 0 && rank_index < 8) {
                board->en_passant_square = rank_index * 8 + file_index;
            }
        }
    }

    board->halfmove_clock = (token_count >= 5) ? atoi(tokens[4]) : 0;
    if (board->halfmove_clock < 0) {
        board->halfmove_clock = 0;
    }
    board->fullmove_number = (token_count >= 6) ? atoi(tokens[5]) : 1;
    if (board->fullmove_number < 1) {
        board->fullmove_number = 1;
    }

    board->zobrist_key = zobrist_compute_key(board);
    return 1;
}

Bitboard board_occupancy(const Board* board, enum Color color) {
    if (board == NULL || color < 0 || color >= COLOR_NB) {
        return 0ULL;
    }

    Bitboard occupancy = 0ULL;
    for (int pt = PIECE_PAWN; pt < PIECE_TYPE_NB; ++pt) {
        occupancy |= board->bitboards[pt + PIECE_TYPE_NB * color];
    }
    return occupancy;
}

int board_is_square_attacked(const Board* board, Square square, enum Color attacker) {
    if (board == NULL) {
        return 0;
    }

    Bitboard occ = board_occupancy(board, COLOR_WHITE) | board_occupancy(board, COLOR_BLACK);

    Bitboard knights = board->bitboards[PIECE_KNIGHT + PIECE_TYPE_NB * attacker];
    for (Bitboard bb = knights; bb; bb &= bb - 1ULL) {
        Square sq = bits_ls1b(bb);
        if (attacks_knight(sq) & (1ULL << square)) {
            return 1;
        }
    }

    Bitboard bishops = board->bitboards[PIECE_BISHOP + PIECE_TYPE_NB * attacker];
    Bitboard queens = board->bitboards[PIECE_QUEEN + PIECE_TYPE_NB * attacker];
    Bitboard sliders = bishops | queens;
    for (Bitboard bb = sliders; bb; bb &= bb - 1ULL) {
        Square sq = bits_ls1b(bb);
        if (attacks_bishop(sq, occ) & (1ULL << square)) {
            return 1;
        }
    }

    Bitboard rooks = board->bitboards[PIECE_ROOK + PIECE_TYPE_NB * attacker];
    sliders = rooks | queens;
    for (Bitboard bb = sliders; bb; bb &= bb - 1ULL) {
        Square sq = bits_ls1b(bb);
        if (attacks_rook(sq, occ) & (1ULL << square)) {
            return 1;
        }
    }

    Bitboard pawns = board->bitboards[PIECE_PAWN + PIECE_TYPE_NB * attacker];
    for (Bitboard bb = pawns; bb; bb &= bb - 1ULL) {
        Square sq = bits_ls1b(bb);
        if (attacks_pawn(sq, attacker) & (1ULL << square)) {
            return 1;
        }
    }

    Bitboard king = board->bitboards[PIECE_KING + PIECE_TYPE_NB * attacker];
    if (king) {
        Square sq = bits_ls1b(king);
        if (attacks_king(sq) & (1ULL << square)) {
            return 1;
        }
    }

    return 0;
}

void board_make_move(Board* board, const Move* move) {
    if (board == NULL || move == NULL) {
        return;
    }

    Piece piece = move->piece;
    enum Color color = board->side_to_move;
    enum Color opponent = (color == COLOR_WHITE) ? COLOR_BLACK : COLOR_WHITE;
    Bitboard from_bb = 1ULL << move->from;
    Bitboard to_bb = 1ULL << move->to;

    if (move->capture != PIECE_NONE) {
        board->bitboards[move->capture + PIECE_TYPE_NB * opponent] &= ~to_bb;
    }

    board->bitboards[piece + PIECE_TYPE_NB * color] &= ~from_bb;
    if (move->promotion != PIECE_NONE) {
        board->bitboards[move->promotion + PIECE_TYPE_NB * color] |= to_bb;
    } else {
        board->bitboards[piece + PIECE_TYPE_NB * color] |= to_bb;
    }

    board->squares[move->from] = PIECE_NONE;
    board->squares[move->to] = (move->promotion != PIECE_NONE) ? move->promotion : piece;

    uint64_t key = board->zobrist_key;
    key ^= zobrist_side_key();
    key ^= zobrist_piece_key(color, piece, move->from);
    if (move->promotion != PIECE_NONE) {
        key ^= zobrist_piece_key(color, move->promotion, move->to);
    } else {
        key ^= zobrist_piece_key(color, piece, move->to);
    }
    if (move->capture != PIECE_NONE) {
        key ^= zobrist_piece_key(opponent, move->capture, move->to);
    }
    board->zobrist_key = key;
    board->side_to_move = opponent;
}

void board_unmake_move(Board* board, const Move* move) {
    if (board == NULL || move == NULL) {
        return;
    }

    enum Color color = (board->side_to_move == COLOR_WHITE) ? COLOR_BLACK : COLOR_WHITE;
    enum Color opponent = board->side_to_move;
    Piece piece = move->piece;
    Bitboard from_bb = 1ULL << move->from;
    Bitboard to_bb = 1ULL << move->to;

    if (move->promotion != PIECE_NONE) {
        board->bitboards[move->promotion + PIECE_TYPE_NB * color] &= ~to_bb;
        board->bitboards[piece + PIECE_TYPE_NB * color] |= from_bb;
    } else {
        board->bitboards[piece + PIECE_TYPE_NB * color] |= from_bb;
        board->bitboards[piece + PIECE_TYPE_NB * color] &= ~to_bb;
    }

    if (move->capture != PIECE_NONE) {
        board->bitboards[move->capture + PIECE_TYPE_NB * opponent] |= to_bb;
    }

    board->squares[move->from] = piece;
    board->squares[move->to] = move->capture;
    board->side_to_move = color;

    uint64_t key = board->zobrist_key;
    key ^= zobrist_side_key();
    key ^= zobrist_piece_key(color, piece, move->from);
    if (move->promotion != PIECE_NONE) {
        key ^= zobrist_piece_key(color, move->promotion, move->to);
    } else {
        key ^= zobrist_piece_key(color, piece, move->to);
    }
    if (move->capture != PIECE_NONE) {
        key ^= zobrist_piece_key(opponent, move->capture, move->to);
    }
    board->zobrist_key = key;
}

