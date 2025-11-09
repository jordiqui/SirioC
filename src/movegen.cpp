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

template <Color Us>
Bitboard pawn_push(Bitboard pawns) {
    if constexpr (Us == Color::White) {
        return pawns << 8;
    } else {
        return pawns >> 8;
    }
}

template <Color Us>
Bitboard pawn_left_attacks(Bitboard pawns) {
    if constexpr (Us == Color::White) {
        return (pawns & not_file_a_mask) << 7;
    } else {
        return (pawns & not_file_a_mask) >> 9;
    }
}

template <Color Us>
Bitboard pawn_right_attacks(Bitboard pawns) {
    if constexpr (Us == Color::White) {
        return (pawns & not_file_h_mask) << 9;
    } else {
        return (pawns & not_file_h_mask) >> 7;
    }
}

template <Color Us>
void generate_pawn_moves_color(const Board &board, Color them, Bitboard occupancy_all,
                               std::vector<Move> &moves, bool tactical_only) {
    constexpr Bitboard promotion_rank = Us == Color::White ? rank_8_mask : rank_1_mask;
    constexpr Bitboard start_rank = Us == Color::White ? rank_2_mask : rank_7_mask;
    constexpr int forward_offset = Us == Color::White ? 8 : -8;
    constexpr int double_offset = forward_offset * 2;
    constexpr int left_offset = Us == Color::White ? 7 : -9;
    constexpr int right_offset = Us == Color::White ? 9 : -7;

    Bitboard pawns = board.pieces(Us, PieceType::Pawn);
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

    Bitboard single_pushes = pawn_push<Us>(pawns) & empty;
    Bitboard promotion_pushes = single_pushes & promotion_rank;
    Bitboard quiet_pushes = single_pushes & ~promotion_rank;
    while (promotion_pushes) {
        int to = pop_lsb(promotion_pushes);
        int from = to - forward_offset;
        emit_promotion(from, to, std::nullopt);
    }

    if (!tactical_only) {
        Bitboard non_promo = quiet_pushes;
        while (non_promo) {
            int to = pop_lsb(non_promo);
            int from = to - forward_offset;
            append_move(moves, Move{from, to, PieceType::Pawn});
        }

        Bitboard double_sources = pawns & start_rank;
        Bitboard first_step = pawn_push<Us>(double_sources) & empty;
        Bitboard double_pushes = pawn_push<Us>(first_step) & empty;
        while (double_pushes) {
            int to = pop_lsb(double_pushes);
            int from = to - double_offset;
            append_move(moves, Move{from, to, PieceType::Pawn});
        }
    }

    auto process_captures = [&](Bitboard capture_mask, int offset) {
        Bitboard promo = capture_mask & promotion_rank;
        Bitboard normal = capture_mask & ~promotion_rank;
        while (promo) {
            int to = pop_lsb(promo);
            int from = to - offset;
            auto captured = captured_piece_on(board, to, them);
            emit_promotion(from, to, captured);
        }
        while (normal) {
            int to = pop_lsb(normal);
            int from = to - offset;
            Move move{from, to, PieceType::Pawn};
            move.captured = captured_piece_on(board, to, them);
            append_move(moves, move);
        }
    };

    Bitboard left_captures = pawn_left_attacks<Us>(pawns) & enemy_occ;
    Bitboard right_captures = pawn_right_attacks<Us>(pawns) & enemy_occ;
    process_captures(left_captures, left_offset);
    process_captures(right_captures, right_offset);

    if (en_passant_mask != 0) {
        Bitboard ep_left = pawn_left_attacks<Us>(pawns) & en_passant_mask;
        while (ep_left) {
            int to = pop_lsb(ep_left);
            int from = to - left_offset;
            Move move{from, to, PieceType::Pawn};
            move.is_en_passant = true;
            move.captured = PieceType::Pawn;
            append_move(moves, move);
        }
        Bitboard ep_right = pawn_right_attacks<Us>(pawns) & en_passant_mask;
        while (ep_right) {
            int to = pop_lsb(ep_right);
            int from = to - right_offset;
            Move move{from, to, PieceType::Pawn};
            move.is_en_passant = true;
            move.captured = PieceType::Pawn;
            append_move(moves, move);
        }
    }
}

void generate_pawn_moves_impl(const Board &board, Color us, Color them, Bitboard occupancy_all,
                              std::vector<Move> &moves, bool tactical_only) {
    if (us == Color::White) {
        generate_pawn_moves_color<Color::White>(board, them, occupancy_all, moves, tactical_only);
    } else {
        generate_pawn_moves_color<Color::Black>(board, them, occupancy_all, moves, tactical_only);
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

