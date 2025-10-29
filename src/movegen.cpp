#include "sirio/movegen.hpp"

#include <array>
#include <exception>
#include <optional>

namespace sirio {

namespace {

std::optional<PieceType> captured_piece_on(const Board &board, int square, Color opponent) {
    auto piece = board.piece_at(square);
    if (piece && piece->first == opponent) {
        return piece->second;
    }
    return std::nullopt;
}

void append_move(std::vector<Move> &moves, Move move) {
    moves.push_back(move);
}

void generate_pawn_moves(const Board &board, Color us, Color them, Bitboard occupancy_all,
                         std::vector<Move> &moves) {
    Bitboard pawns = board.pieces(us, PieceType::Pawn);
    auto en_passant = board.en_passant_square();

    while (pawns) {
        int from = pop_lsb(pawns);
        int from_file = file_of(from);
        int from_rank = rank_of(from);

        if (us == Color::White) {
            int forward = from + 8;
            if (forward < 64 && (occupancy_all & one_bit(forward)) == 0) {
                if (from_rank == 6) {
                    for (PieceType promo : {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight}) {
                        Move move{from, forward, PieceType::Pawn};
                        move.promotion = promo;
                        append_move(moves, move);
                    }
                } else {
                    append_move(moves, Move{from, forward, PieceType::Pawn});
                    if (from_rank == 1) {
                        int double_forward = from + 16;
                        if ((occupancy_all & one_bit(double_forward)) == 0) {
                            Move move{from, double_forward, PieceType::Pawn};
                            append_move(moves, move);
                        }
                    }
                }
            }

            if (from_file > 0) {
                int target = from + 7;
                if (target < 64) {
                    Move move{from, target, PieceType::Pawn};
                    if (auto capture = captured_piece_on(board, target, them)) {
                        move.captured = capture;
                        if (from_rank == 6) {
                            for (PieceType promo : {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight}) {
                                Move promo_move = move;
                                promo_move.promotion = promo;
                                append_move(moves, promo_move);
                            }
                        } else {
                            append_move(moves, move);
                        }
                    } else if (en_passant && *en_passant == target) {
                        move.is_en_passant = true;
                        move.captured = PieceType::Pawn;
                        append_move(moves, move);
                    }
                }
            }

            if (from_file < 7) {
                int target = from + 9;
                if (target < 64) {
                    Move move{from, target, PieceType::Pawn};
                    if (auto capture = captured_piece_on(board, target, them)) {
                        move.captured = capture;
                        if (from_rank == 6) {
                            for (PieceType promo : {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight}) {
                                Move promo_move = move;
                                promo_move.promotion = promo;
                                append_move(moves, promo_move);
                            }
                        } else {
                            append_move(moves, move);
                        }
                    } else if (en_passant && *en_passant == target) {
                        move.is_en_passant = true;
                        move.captured = PieceType::Pawn;
                        append_move(moves, move);
                    }
                }
            }
        } else {
            int forward = from - 8;
            if (forward >= 0 && (occupancy_all & one_bit(forward)) == 0) {
                if (from_rank == 1) {
                    for (PieceType promo : {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight}) {
                        Move move{from, forward, PieceType::Pawn};
                        move.promotion = promo;
                        append_move(moves, move);
                    }
                } else {
                    append_move(moves, Move{from, forward, PieceType::Pawn});
                    if (from_rank == 6) {
                        int double_forward = from - 16;
                        if ((occupancy_all & one_bit(double_forward)) == 0) {
                            Move move{from, double_forward, PieceType::Pawn};
                            append_move(moves, move);
                        }
                    }
                }
            }

            if (from_file > 0) {
                int target = from - 9;
                if (target >= 0) {
                    Move move{from, target, PieceType::Pawn};
                    if (auto capture = captured_piece_on(board, target, them)) {
                        move.captured = capture;
                        if (from_rank == 1) {
                            for (PieceType promo : {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight}) {
                                Move promo_move = move;
                                promo_move.promotion = promo;
                                append_move(moves, promo_move);
                            }
                        } else {
                            append_move(moves, move);
                        }
                    } else if (en_passant && *en_passant == target) {
                        move.is_en_passant = true;
                        move.captured = PieceType::Pawn;
                        append_move(moves, move);
                    }
                }
            }

            if (from_file < 7) {
                int target = from - 7;
                if (target >= 0) {
                    Move move{from, target, PieceType::Pawn};
                    if (auto capture = captured_piece_on(board, target, them)) {
                        move.captured = capture;
                        if (from_rank == 1) {
                            for (PieceType promo : {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight}) {
                                Move promo_move = move;
                                promo_move.promotion = promo;
                                append_move(moves, promo_move);
                            }
                        } else {
                            append_move(moves, move);
                        }
                    } else if (en_passant && *en_passant == target) {
                        move.is_en_passant = true;
                        move.captured = PieceType::Pawn;
                        append_move(moves, move);
                    }
                }
            }
        }
    }
}

void generate_leaper_moves(const Board &board, Color us, Color them, Bitboard occupancy_us,
                           Bitboard occupancy_them, PieceType piece_type,
                           Bitboard (*attack_function)(int), std::vector<Move> &moves) {
    Bitboard pieces = board.pieces(us, piece_type);
    while (pieces) {
        int from = pop_lsb(pieces);
        Bitboard attacks = attack_function(from) & ~occupancy_us;
        Bitboard quiet = attacks & ~occupancy_them;
        while (quiet) {
            int to = pop_lsb(quiet);
            append_move(moves, Move{from, to, piece_type});
        }
        Bitboard captures = attacks & occupancy_them;
        while (captures) {
            int to = pop_lsb(captures);
            Move move{from, to, piece_type};
            move.captured = captured_piece_on(board, to, them);
            append_move(moves, move);
        }
    }
}

void generate_leaper_captures(const Board &board, Color us, Color them, Bitboard occupancy_them,
                              PieceType piece_type, Bitboard (*attack_function)(int),
                              std::vector<Move> &moves) {
    Bitboard pieces = board.pieces(us, piece_type);
    while (pieces) {
        int from = pop_lsb(pieces);
        Bitboard captures = attack_function(from) & occupancy_them;
        while (captures) {
            int to = pop_lsb(captures);
            Move move{from, to, piece_type};
            move.captured = captured_piece_on(board, to, them);
            append_move(moves, move);
        }
    }
}

void generate_slider_moves(const Board &board, Color us, Color them, Bitboard occupancy_us,
                           Bitboard occupancy_all, PieceType piece_type,
                           Bitboard (*attack_function)(int, Bitboard),
                           std::vector<Move> &moves) {
    Bitboard pieces = board.pieces(us, piece_type);
    Bitboard occupancy_them = board.occupancy(them);
    while (pieces) {
        int from = pop_lsb(pieces);
        Bitboard attacks = attack_function(from, occupancy_all) & ~occupancy_us;
        Bitboard quiet = attacks & ~occupancy_them;
        while (quiet) {
            int to = pop_lsb(quiet);
            append_move(moves, Move{from, to, piece_type});
        }
        Bitboard captures = attacks & occupancy_them;
        while (captures) {
            int to = pop_lsb(captures);
            Move move{from, to, piece_type};
            move.captured = captured_piece_on(board, to, them);
            append_move(moves, move);
        }
    }
}

void generate_slider_captures(const Board &board, Color us, Color them, Bitboard occupancy_all,
                              PieceType piece_type, Bitboard (*attack_function)(int, Bitboard),
                              std::vector<Move> &moves) {
    Bitboard pieces = board.pieces(us, piece_type);
    Bitboard occupancy_them = board.occupancy(them);
    while (pieces) {
        int from = pop_lsb(pieces);
        Bitboard captures = attack_function(from, occupancy_all) & occupancy_them;
        while (captures) {
            int to = pop_lsb(captures);
            Move move{from, to, piece_type};
            move.captured = captured_piece_on(board, to, them);
            append_move(moves, move);
        }
    }
}

void generate_castling_moves(const Board &board, Color us, Color them, std::vector<Move> &moves) {
    const CastlingRights &rights = board.castling_rights();
    int king_sq = board.king_square(us);
    if (king_sq < 0) {
        return;
    }
    if (board.is_square_attacked(king_sq, them)) {
        return;
    }

    Bitboard all_occ = board.occupancy();
    if (us == Color::White) {
        if (rights.white_kingside) {
            if ((all_occ & (one_bit(5) | one_bit(6))) == 0 &&
                !board.is_square_attacked(5, them) && !board.is_square_attacked(6, them)) {
                Move move{king_sq, 6, PieceType::King};
                move.is_castling = true;
                append_move(moves, move);
            }
        }
        if (rights.white_queenside) {
            if ((all_occ & (one_bit(1) | one_bit(2) | one_bit(3))) == 0 &&
                !board.is_square_attacked(2, them) && !board.is_square_attacked(3, them)) {
                Move move{king_sq, 2, PieceType::King};
                move.is_castling = true;
                append_move(moves, move);
            }
        }
    } else {
        if (rights.black_kingside) {
            if ((all_occ & (one_bit(61) | one_bit(62))) == 0 &&
                !board.is_square_attacked(61, them) && !board.is_square_attacked(62, them)) {
                Move move{king_sq, 62, PieceType::King};
                move.is_castling = true;
                append_move(moves, move);
            }
        }
        if (rights.black_queenside) {
            if ((all_occ & (one_bit(57) | one_bit(58) | one_bit(59))) == 0 &&
                !board.is_square_attacked(58, them) && !board.is_square_attacked(59, them)) {
                Move move{king_sq, 58, PieceType::King};
                move.is_castling = true;
                append_move(moves, move);
            }
        }
    }
}

void generate_pawn_tactical_moves(const Board &board, Color us, Color them,
                                  Bitboard occupancy_all, std::vector<Move> &moves) {
    Bitboard pawns = board.pieces(us, PieceType::Pawn);
    auto en_passant = board.en_passant_square();
    while (pawns) {
        int from = pop_lsb(pawns);
        int from_file = file_of(from);
        int from_rank = rank_of(from);

        if (us == Color::White) {
            int forward = from + 8;
            if (from_rank == 6 && forward < 64 && (occupancy_all & one_bit(forward)) == 0) {
                for (PieceType promo : {PieceType::Queen, PieceType::Rook, PieceType::Bishop,
                                        PieceType::Knight}) {
                    Move move{from, forward, PieceType::Pawn};
                    move.promotion = promo;
                    append_move(moves, move);
                }
            }

            if (from_file > 0) {
                int target = from + 7;
                if (target < 64) {
                    Move move{from, target, PieceType::Pawn};
                    if (auto capture = captured_piece_on(board, target, them)) {
                        move.captured = capture;
                        if (from_rank == 6) {
                            for (PieceType promo : {PieceType::Queen, PieceType::Rook,
                                                    PieceType::Bishop, PieceType::Knight}) {
                                Move promo_move = move;
                                promo_move.promotion = promo;
                                append_move(moves, promo_move);
                            }
                        } else {
                            append_move(moves, move);
                        }
                    } else if (en_passant && *en_passant == target) {
                        move.is_en_passant = true;
                        move.captured = PieceType::Pawn;
                        append_move(moves, move);
                    }
                }
            }

            if (from_file < 7) {
                int target = from + 9;
                if (target < 64) {
                    Move move{from, target, PieceType::Pawn};
                    if (auto capture = captured_piece_on(board, target, them)) {
                        move.captured = capture;
                        if (from_rank == 6) {
                            for (PieceType promo : {PieceType::Queen, PieceType::Rook,
                                                    PieceType::Bishop, PieceType::Knight}) {
                                Move promo_move = move;
                                promo_move.promotion = promo;
                                append_move(moves, promo_move);
                            }
                        } else {
                            append_move(moves, move);
                        }
                    } else if (en_passant && *en_passant == target) {
                        move.is_en_passant = true;
                        move.captured = PieceType::Pawn;
                        append_move(moves, move);
                    }
                }
            }
        } else {
            int forward = from - 8;
            if (from_rank == 1 && forward >= 0 && (occupancy_all & one_bit(forward)) == 0) {
                for (PieceType promo : {PieceType::Queen, PieceType::Rook, PieceType::Bishop,
                                        PieceType::Knight}) {
                    Move move{from, forward, PieceType::Pawn};
                    move.promotion = promo;
                    append_move(moves, move);
                }
            }

            if (from_file > 0) {
                int target = from - 9;
                if (target >= 0) {
                    Move move{from, target, PieceType::Pawn};
                    if (auto capture = captured_piece_on(board, target, them)) {
                        move.captured = capture;
                        if (from_rank == 1) {
                            for (PieceType promo : {PieceType::Queen, PieceType::Rook,
                                                    PieceType::Bishop, PieceType::Knight}) {
                                Move promo_move = move;
                                promo_move.promotion = promo;
                                append_move(moves, promo_move);
                            }
                        } else {
                            append_move(moves, move);
                        }
                    } else if (en_passant && *en_passant == target) {
                        move.is_en_passant = true;
                        move.captured = PieceType::Pawn;
                        append_move(moves, move);
                    }
                }
            }

            if (from_file < 7) {
                int target = from - 7;
                if (target >= 0) {
                    Move move{from, target, PieceType::Pawn};
                    if (auto capture = captured_piece_on(board, target, them)) {
                        move.captured = capture;
                        if (from_rank == 1) {
                            for (PieceType promo : {PieceType::Queen, PieceType::Rook,
                                                    PieceType::Bishop, PieceType::Knight}) {
                                Move promo_move = move;
                                promo_move.promotion = promo;
                                append_move(moves, promo_move);
                            }
                        } else {
                            append_move(moves, move);
                        }
                    } else if (en_passant && *en_passant == target) {
                        move.is_en_passant = true;
                        move.captured = PieceType::Pawn;
                        append_move(moves, move);
                    }
                }
            }
        }
    }
}

}  // namespace

std::vector<Move> generate_pseudo_legal_moves(const Board &board) {
    std::vector<Move> moves;
    const Color us = board.side_to_move();
    const Color them = opposite(us);
    const Bitboard occupancy_all = board.occupancy();
    const Bitboard occupancy_us = board.occupancy(us);
    const Bitboard occupancy_them = board.occupancy(them);

    generate_pawn_moves(board, us, them, occupancy_all, moves);
    generate_leaper_moves(board, us, them, occupancy_us, occupancy_them, PieceType::Knight,
                          knight_attacks, moves);
    generate_slider_moves(board, us, them, occupancy_us, occupancy_all, PieceType::Bishop,
                          bishop_attacks, moves);
    generate_slider_moves(board, us, them, occupancy_us, occupancy_all, PieceType::Rook,
                          rook_attacks, moves);
    generate_slider_moves(board, us, them, occupancy_us, occupancy_all, PieceType::Queen,
                          queen_attacks, moves);
    generate_leaper_moves(board, us, them, occupancy_us, occupancy_them, PieceType::King,
                          king_attacks, moves);
    generate_castling_moves(board, us, them, moves);

    return moves;
}

std::vector<Move> generate_pseudo_legal_tactical_moves(const Board &board) {
    std::vector<Move> moves;
    const Color us = board.side_to_move();
    const Color them = opposite(us);
    const Bitboard occupancy_all = board.occupancy();
    const Bitboard occupancy_them = board.occupancy(them);

    generate_pawn_tactical_moves(board, us, them, occupancy_all, moves);
    generate_leaper_captures(board, us, them, occupancy_them, PieceType::Knight, knight_attacks,
                             moves);
    generate_slider_captures(board, us, them, occupancy_all, PieceType::Bishop, bishop_attacks,
                             moves);
    generate_slider_captures(board, us, them, occupancy_all, PieceType::Rook, rook_attacks, moves);
    generate_slider_captures(board, us, them, occupancy_all, PieceType::Queen, queen_attacks,
                             moves);
    generate_leaper_captures(board, us, them, occupancy_them, PieceType::King, king_attacks, moves);

    return moves;
}

std::vector<Move> generate_legal_moves(Board &board) {
    std::vector<Move> legal_moves;
    auto pseudo = generate_pseudo_legal_moves(board);
    legal_moves.reserve(pseudo.size());
    for (const Move &move : pseudo) {
        Board::UndoState undo;
        bool legal = true;
        try {
            board.make_move(move, undo);
        } catch (const std::exception &) {
            legal = false;
        }
        if (legal) {
            Color mover = opposite(board.side_to_move());
            if (board.king_square(mover) >= 0 && board.in_check(mover)) {
                legal = false;
            }
        }
        if (legal) {
            legal_moves.push_back(move);
        }
        board.undo_move(move, undo);
    }

    return legal_moves;
}

std::vector<Move> generate_legal_moves(const Board &board) {
    Board copy = board;
    return generate_legal_moves(copy);
}

}  // namespace sirio

