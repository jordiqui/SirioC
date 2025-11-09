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

void generate_pawn_moves_impl(const Board &board, Color us, Color them, Bitboard occupancy_all,
                              std::vector<Move> &moves, bool tactical_only) {
    Bitboard pawns = board.pieces(us, PieceType::Pawn);
    Bitboard enemy_occ = board.occupancy(them);
    Bitboard empty = ~occupancy_all;
    auto en_passant = board.en_passant_square();
    Bitboard en_passant_mask = en_passant ? one_bit(*en_passant) : 0ULL;

    auto emit_promotion = [&](int from, int to, std::optional<PieceType> captured_piece) {
        for (PieceType promo : {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight}) {
            Move move{from, to, PieceType::Pawn};
            move.promotion = promo;
            if (captured_piece.has_value()) {
                move.captured = captured_piece;
            }
            append_move(moves, move);
        }
    };

    if (us == Color::White) {
        Bitboard promotion_pushes = ((pawns & rank_7_mask) << 8) & empty;
        while (promotion_pushes) {
            int to = pop_lsb(promotion_pushes);
            int from = to - 8;
            emit_promotion(from, to, std::nullopt);
        }

        if (!tactical_only) {
            Bitboard single_pushes = ((pawns & ~rank_7_mask) << 8) & empty;
            while (single_pushes) {
                int to = pop_lsb(single_pushes);
                int from = to - 8;
                append_move(moves, Move{from, to, PieceType::Pawn});
            }

            Bitboard double_pushes = (((pawns & rank_2_mask) << 8) & empty);
            double_pushes = (double_pushes << 8) & empty;
            while (double_pushes) {
                int to = pop_lsb(double_pushes);
                int from = to - 16;
                append_move(moves, Move{from, to, PieceType::Pawn});
            }
        }

        Bitboard left_captures = ((pawns & not_file_a_mask) << 7) & enemy_occ;
        while (left_captures) {
            int to = pop_lsb(left_captures);
            int from = to - 7;
            auto captured = captured_piece_on(board, to, them);
            if (to >= 56) {
                emit_promotion(from, to, captured);
            } else {
                Move move{from, to, PieceType::Pawn};
                move.captured = captured;
                append_move(moves, move);
            }
        }

        Bitboard right_captures = ((pawns & not_file_h_mask) << 9) & enemy_occ;
        while (right_captures) {
            int to = pop_lsb(right_captures);
            int from = to - 9;
            auto captured = captured_piece_on(board, to, them);
            if (to >= 56) {
                emit_promotion(from, to, captured);
            } else {
                Move move{from, to, PieceType::Pawn};
                move.captured = captured;
                append_move(moves, move);
            }
        }

        if (en_passant_mask != 0) {
            Bitboard ep_left = ((pawns & not_file_a_mask) << 7) & en_passant_mask;
            while (ep_left) {
                int to = pop_lsb(ep_left);
                int from = to - 7;
                Move move{from, to, PieceType::Pawn};
                move.is_en_passant = true;
                move.captured = PieceType::Pawn;
                append_move(moves, move);
            }
            Bitboard ep_right = ((pawns & not_file_h_mask) << 9) & en_passant_mask;
            while (ep_right) {
                int to = pop_lsb(ep_right);
                int from = to - 9;
                Move move{from, to, PieceType::Pawn};
                move.is_en_passant = true;
                move.captured = PieceType::Pawn;
                append_move(moves, move);
            }
        }
    } else {
        Bitboard promotion_pushes = ((pawns & rank_2_mask) >> 8) & empty;
        while (promotion_pushes) {
            int to = pop_lsb(promotion_pushes);
            int from = to + 8;
            emit_promotion(from, to, std::nullopt);
        }

        if (!tactical_only) {
            Bitboard single_pushes = ((pawns & ~rank_2_mask) >> 8) & empty;
            while (single_pushes) {
                int to = pop_lsb(single_pushes);
                int from = to + 8;
                append_move(moves, Move{from, to, PieceType::Pawn});
            }

            Bitboard double_pushes = (((pawns & rank_7_mask) >> 8) & empty);
            double_pushes = (double_pushes >> 8) & empty;
            while (double_pushes) {
                int to = pop_lsb(double_pushes);
                int from = to + 16;
                append_move(moves, Move{from, to, PieceType::Pawn});
            }
        }

        Bitboard left_captures = ((pawns & not_file_a_mask) >> 9) & enemy_occ;
        while (left_captures) {
            int to = pop_lsb(left_captures);
            int from = to + 9;
            auto captured = captured_piece_on(board, to, them);
            if (to <= 7) {
                emit_promotion(from, to, captured);
            } else {
                Move move{from, to, PieceType::Pawn};
                move.captured = captured;
                append_move(moves, move);
            }
        }

        Bitboard right_captures = ((pawns & not_file_h_mask) >> 7) & enemy_occ;
        while (right_captures) {
            int to = pop_lsb(right_captures);
            int from = to + 7;
            auto captured = captured_piece_on(board, to, them);
            if (to <= 7) {
                emit_promotion(from, to, captured);
            } else {
                Move move{from, to, PieceType::Pawn};
                move.captured = captured;
                append_move(moves, move);
            }
        }

        if (en_passant_mask != 0) {
            Bitboard ep_left = ((pawns & not_file_a_mask) >> 9) & en_passant_mask;
            while (ep_left) {
                int to = pop_lsb(ep_left);
                int from = to + 9;
                Move move{from, to, PieceType::Pawn};
                move.is_en_passant = true;
                move.captured = PieceType::Pawn;
                append_move(moves, move);
            }
            Bitboard ep_right = ((pawns & not_file_h_mask) >> 7) & en_passant_mask;
            while (ep_right) {
                int to = pop_lsb(ep_right);
                int from = to + 7;
                Move move{from, to, PieceType::Pawn};
                move.is_en_passant = true;
                move.captured = PieceType::Pawn;
                append_move(moves, move);
            }
        }
    }
}

void generate_pawn_moves(const Board &board, Color us, Color them, Bitboard occupancy_all,
                         std::vector<Move> &moves) {
    generate_pawn_moves_impl(board, us, them, occupancy_all, moves, false);
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
    generate_pawn_moves_impl(board, us, them, occupancy_all, moves, true);
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

std::vector<Move> generate_pseudo_legal_quiet_checks(Board &board) {
    auto pseudo = generate_pseudo_legal_moves(board);
    std::vector<Move> quiet_checks;
    quiet_checks.reserve(pseudo.size());

    for (const Move &move : pseudo) {
        if (move.captured.has_value() || move.promotion.has_value() || move.is_en_passant ||
            move.is_castling) {
            continue;
        }

        Board::UndoState undo;
        try {
            board.make_move(move, undo);
        } catch (const std::exception &) {
            continue;
        }

        Color mover = opposite(board.side_to_move());
        if (board.king_square(mover) >= 0 && board.in_check(mover)) {
            board.undo_move(move, undo);
            continue;
        }

        if (board.in_check(board.side_to_move())) {
            quiet_checks.push_back(move);
        }

        board.undo_move(move, undo);
    }

    return quiet_checks;
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

