#include "movegen.h"
#include "attacks.h"
#include "bits.h"

void movegen_generate_legal_moves(const Board* board, MoveList* list) {
    if (board == NULL || list == NULL) {
        return;
    }

    list->size = 0;

    enum Color color = board->side_to_move;
    Bitboard occupancy = board_occupancy(board, COLOR_WHITE) | board_occupancy(board, COLOR_BLACK);
    Bitboard pieces = board_occupancy(board, color);

    while (pieces) {
        Bitboard lsb = bits_pop_lsb(&pieces);
        Square from = bits_ls1b(lsb);
        Piece piece = board->squares[from];
        Bitboard attacks = 0ULL;

        switch (piece) {
            case PIECE_PAWN:
                attacks = attacks_pawn(from, color);
                break;
            case PIECE_KNIGHT:
                attacks = attacks_knight(from);
                break;
            case PIECE_BISHOP:
                attacks = attacks_bishop(from, occupancy);
                break;
            case PIECE_ROOK:
                attacks = attacks_rook(from, occupancy);
                break;
            case PIECE_QUEEN:
                attacks = attacks_queen(from, occupancy);
                break;
            case PIECE_KING:
                attacks = attacks_king(from);
                break;
            default:
                break;
        }

        Bitboard targets = attacks & ~board_occupancy(board, color);
        while (targets) {
            Bitboard to_bb = bits_pop_lsb(&targets);
            Square to = bits_ls1b(to_bb);
            Move move = move_create(from, to, piece, board->squares[to], PIECE_NONE, 0);
            list->moves[list->size++] = move;
        }
    }
}

