#include "engine/core/board.hpp"
#include "engine/core/fen.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

using namespace engine;

static uint64_t bitboard_for_square(char file, char rank) {
    int file_idx = file - 'a';
    int rank_idx = rank - '1';
    return 1ULL << (rank_idx * 8 + file_idx);
}

int main() {
    Board board;
    assert(board.last_fen() == Board::kStartposFEN);
    assert(board.white_to_move());
    assert(board.castling_rights() == (Board::CASTLE_WHITE_KINGSIDE |
                                       Board::CASTLE_WHITE_QUEENSIDE |
                                       Board::CASTLE_BLACK_KINGSIDE |
                                       Board::CASTLE_BLACK_QUEENSIDE));
    assert(board.en_passant_square() == Board::INVALID_SQUARE);
    assert(board.halfmove_clock() == 0);
    assert(board.fullmove_number() == 1);

    const auto& bb = board.piece_bitboards();
    assert(bb[Board::WHITE_PAWN] == 0x000000000000FF00ULL);
    assert(bb[Board::BLACK_PAWN] == 0x00FF000000000000ULL);
    assert(bb[Board::WHITE_KING] == bitboard_for_square('e', '1'));
    assert(bb[Board::BLACK_KING] == bitboard_for_square('e', '8'));

    std::string fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
    assert(fen::is_valid_fen(fen));
    assert(board.set_fen(fen));
    assert(!board.white_to_move());
    assert(board.en_passant_square() == 20); // e3
    assert(board.halfmove_clock() == 0);
    assert(board.fullmove_number() == 1);
    assert(board.piece_on(28) == 'P'); // e4

    std::string bad_fen = "8/8/8/8/8/8/8/8 w - -";
    assert(!fen::is_valid_fen(bad_fen));

    std::cout << "All Board FEN tests passed.\n";
    return 0;
}
