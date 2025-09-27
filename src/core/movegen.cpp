#include "engine/core/board.hpp"
#include <cctype>

namespace engine {

namespace {
constexpr int knight_offsets[8][2] = {
    {1, 2}, {2, 1}, {2, -1}, {1, -2},
    {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2},
};

constexpr int king_offsets[8][2] = {
    {1, 0},  {1, 1},  {0, 1},  {-1, 1},
    {-1, 0}, {-1, -1}, {0, -1}, {1, -1},
};

constexpr int bishop_dirs[4][2] = {
    {1, 1},  {1, -1},
    {-1, 1}, {-1, -1},
};

constexpr int rook_dirs[4][2] = {
    {1, 0},  {0, 1},
    {-1, 0}, {0, -1},
};

inline bool on_board(int file, int rank) {
    return file >= 0 && file < 8 && rank >= 0 && rank < 8;
}

} // namespace

std::vector<Move> Board::generate_legal_moves() const {
    std::vector<Move> pseudo;
    generate_pseudo_legal_moves(pseudo);
    std::vector<Move> legal;
    legal.reserve(pseudo.size());
    bool moving_white = stm_white_;
    for (Move m : pseudo) {
        Board copy(*this);
        copy.do_move(m);
        if (!copy.in_check(moving_white)) {
            legal.push_back(m);
        }
    }
    return legal;
}

void Board::generate_pseudo_legal_moves(std::vector<Move>& moves) const {
    for (int sq = 0; sq < 64; ++sq) {
        char piece = squares_[sq];
        if (is_empty(piece)) continue;
        if (stm_white_ && !is_white_piece(piece)) continue;
        if (!stm_white_ && !is_black_piece(piece)) continue;
        switch (std::tolower(static_cast<unsigned char>(piece))) {
            case 'p':
                generate_pawn_moves(sq, moves);
                break;
            case 'n':
                generate_knight_moves(sq, moves);
                break;
            case 'b':
                generate_bishop_moves(sq, moves);
                break;
            case 'r':
                generate_rook_moves(sq, moves);
                break;
            case 'q':
                generate_queen_moves(sq, moves);
                break;
            case 'k':
                generate_king_moves(sq, moves);
                break;
            default:
                break;
        }
    }
}

void Board::generate_pawn_moves(int sq, std::vector<Move>& moves) const {
    bool white = stm_white_;
    int file = file_of(sq);
    int rank = rank_of(sq);
    int forward_dir = white ? 1 : -1;
    int start_rank = white ? 1 : 6;
    int promotion_rank = white ? 6 : 1;

    int one_forward_rank = rank + forward_dir;
    if (on_board(file, one_forward_rank)) {
        int one_forward_sq = to_index(file, one_forward_rank);
        if (is_empty(squares_[one_forward_sq])) {
            if (rank == promotion_rank) {
                for (int promo = 1; promo <= 4; ++promo) {
                    moves.push_back(::engine::make_move(sq, one_forward_sq, promo));
                }
            } else {
                moves.push_back(::engine::make_move(sq, one_forward_sq));
                if (rank == start_rank) {
                    int two_forward_rank = rank + 2 * forward_dir;
                    if (on_board(file, two_forward_rank)) {
                        int two_forward_sq = to_index(file, two_forward_rank);
                        if (is_empty(squares_[two_forward_sq])) {
                            moves.push_back(::engine::make_move(sq, two_forward_sq, 0, false, true));
                        }
                    }
                }
            }
        }
    }

    for (int df : {-1, 1}) {
        int target_file = file + df;
        int target_rank = rank + forward_dir;
        if (!on_board(target_file, target_rank)) continue;
        int target_sq = to_index(target_file, target_rank);
        char target_piece = squares_[target_sq];
        bool is_capture = !is_empty(target_piece) &&
                          ((white && is_black_piece(target_piece)) ||
                           (!white && is_white_piece(target_piece)));
        if (is_capture) {
            if (rank == promotion_rank) {
                for (int promo = 1; promo <= 4; ++promo) {
                    moves.push_back(::engine::make_move(sq, target_sq, promo, true));
                }
            } else {
                moves.push_back(::engine::make_move(sq, target_sq, 0, true));
            }
        }
        if (en_passant_square_ == target_sq) {
            moves.push_back(::engine::make_move(sq, target_sq, 0, true, false, true));
        }
    }
}

void Board::generate_knight_moves(int sq, std::vector<Move>& moves) const {
    bool white = stm_white_;
    int file = file_of(sq);
    int rank = rank_of(sq);
    for (auto [df, dr] : knight_offsets) {
        int nf = file + df;
        int nr = rank + dr;
        if (!on_board(nf, nr)) continue;
        int target_sq = to_index(nf, nr);
        char target_piece = squares_[target_sq];
        if (is_empty(target_piece) || (white && is_black_piece(target_piece)) ||
            (!white && is_white_piece(target_piece))) {
            bool capture = !is_empty(target_piece);
            moves.push_back(::engine::make_move(sq, target_sq, 0, capture));
        }
    }
}

void Board::generate_bishop_moves(int sq, std::vector<Move>& moves) const {
    bool white = stm_white_;
    int file = file_of(sq);
    int rank = rank_of(sq);
    for (auto [df, dr] : bishop_dirs) {
        int nf = file + df;
        int nr = rank + dr;
        while (on_board(nf, nr)) {
            int target_sq = to_index(nf, nr);
            char target_piece = squares_[target_sq];
            if (is_empty(target_piece)) {
                moves.push_back(::engine::make_move(sq, target_sq));
            } else {
                if ((white && is_black_piece(target_piece)) ||
                    (!white && is_white_piece(target_piece))) {
                    moves.push_back(::engine::make_move(sq, target_sq, 0, true));
                }
                break;
            }
            nf += df;
            nr += dr;
        }
    }
}

void Board::generate_rook_moves(int sq, std::vector<Move>& moves) const {
    bool white = stm_white_;
    int file = file_of(sq);
    int rank = rank_of(sq);
    for (auto [df, dr] : rook_dirs) {
        int nf = file + df;
        int nr = rank + dr;
        while (on_board(nf, nr)) {
            int target_sq = to_index(nf, nr);
            char target_piece = squares_[target_sq];
            if (is_empty(target_piece)) {
                moves.push_back(::engine::make_move(sq, target_sq));
            } else {
                if ((white && is_black_piece(target_piece)) ||
                    (!white && is_white_piece(target_piece))) {
                    moves.push_back(::engine::make_move(sq, target_sq, 0, true));
                }
                break;
            }
            nf += df;
            nr += dr;
        }
    }
}

void Board::generate_queen_moves(int sq, std::vector<Move>& moves) const {
    generate_bishop_moves(sq, moves);
    generate_rook_moves(sq, moves);
}

void Board::generate_king_moves(int sq, std::vector<Move>& moves) const {
    bool white = stm_white_;
    int file = file_of(sq);
    int rank = rank_of(sq);
    for (auto [df, dr] : king_offsets) {
        int nf = file + df;
        int nr = rank + dr;
        if (!on_board(nf, nr)) continue;
        int target_sq = to_index(nf, nr);
        char target_piece = squares_[target_sq];
        if (is_empty(target_piece) || (white && is_black_piece(target_piece)) ||
            (!white && is_white_piece(target_piece))) {
            bool capture = !is_empty(target_piece);
            moves.push_back(::engine::make_move(sq, target_sq, 0, capture));
        }
    }

    // Castling
    if (white) {
        int e1 = to_index(4, 0);
        int f1 = to_index(5, 0);
        int g1 = to_index(6, 0);
        int d1 = to_index(3, 0);
        int c1 = to_index(2, 0);
        if ((castling_rights_ & CASTLE_WHITE_K) &&
            is_empty(squares_[f1]) && is_empty(squares_[g1]) &&
            !in_check(true) && !is_square_attacked(f1, false) &&
            !is_square_attacked(g1, false)) {
            moves.push_back(::engine::make_move(e1, g1, 0, false, false, false, true));
        }
        if ((castling_rights_ & CASTLE_WHITE_Q) &&
            is_empty(squares_[d1]) && is_empty(squares_[c1]) &&
            is_empty(squares_[to_index(1, 0)]) &&
            !in_check(true) && !is_square_attacked(d1, false) &&
            !is_square_attacked(c1, false)) {
            moves.push_back(::engine::make_move(e1, c1, 0, false, false, false, true));
        }
    } else {
        int e8 = to_index(4, 7);
        int f8 = to_index(5, 7);
        int g8 = to_index(6, 7);
        int d8 = to_index(3, 7);
        int c8 = to_index(2, 7);
        if ((castling_rights_ & CASTLE_BLACK_K) &&
            is_empty(squares_[f8]) && is_empty(squares_[g8]) &&
            !in_check(false) && !is_square_attacked(f8, true) &&
            !is_square_attacked(g8, true)) {
            moves.push_back(::engine::make_move(e8, g8, 0, false, false, false, true));
        }
        if ((castling_rights_ & CASTLE_BLACK_Q) &&
            is_empty(squares_[d8]) && is_empty(squares_[c8]) &&
            is_empty(squares_[to_index(1, 7)]) &&
            !in_check(false) && !is_square_attacked(d8, true) &&
            !is_square_attacked(c8, true)) {
            moves.push_back(::engine::make_move(e8, c8, 0, false, false, false, true));
        }
    }
}

bool Board::in_check(bool white) const {
    int king_sq = find_king_square(white);
    if (king_sq == -1) return false;
    return is_square_attacked(king_sq, !white);
}

int Board::find_king_square(bool white) const {
    char king_char = white ? 'K' : 'k';
    for (int sq = 0; sq < 64; ++sq) {
        if (squares_[sq] == king_char) return sq;
    }
    return -1;
}

bool Board::is_square_attacked(int sq, bool by_white) const {
    int file = file_of(sq);
    int rank = rank_of(sq);

    // Pawns
    int pawn_dir = by_white ? -1 : 1;
    for (int df : {-1, 1}) {
        int nf = file + df;
        int nr = rank + pawn_dir;
        if (!on_board(nf, nr)) continue;
        int target = to_index(nf, nr);
        char piece = squares_[target];
        if (by_white) {
            if (piece == 'P') return true;
        } else {
            if (piece == 'p') return true;
        }
    }

    // Knights
    for (auto [df, dr] : knight_offsets) {
        int nf = file + df;
        int nr = rank + dr;
        if (!on_board(nf, nr)) continue;
        char piece = squares_[to_index(nf, nr)];
        if (by_white && piece == 'N') return true;
        if (!by_white && piece == 'n') return true;
    }

    // Bishops/Queens (diagonals)
    for (auto [df, dr] : bishop_dirs) {
        int nf = file + df;
        int nr = rank + dr;
        while (on_board(nf, nr)) {
            char piece = squares_[to_index(nf, nr)];
            if (!is_empty(piece)) {
                if (by_white && (piece == 'B' || piece == 'Q')) return true;
                if (!by_white && (piece == 'b' || piece == 'q')) return true;
                break;
            }
            nf += df;
            nr += dr;
        }
    }

    // Rooks/Queens (straight)
    for (auto [df, dr] : rook_dirs) {
        int nf = file + df;
        int nr = rank + dr;
        while (on_board(nf, nr)) {
            char piece = squares_[to_index(nf, nr)];
            if (!is_empty(piece)) {
                if (by_white && (piece == 'R' || piece == 'Q')) return true;
                if (!by_white && (piece == 'r' || piece == 'q')) return true;
                break;
            }
            nf += df;
            nr += dr;
        }
    }

    // King
    for (auto [df, dr] : king_offsets) {
        int nf = file + df;
        int nr = rank + dr;
        if (!on_board(nf, nr)) continue;
        char piece = squares_[to_index(nf, nr)];
        if (by_white && piece == 'K') return true;
        if (!by_white && piece == 'k') return true;
    }

    return false;
}

void Board::do_move(Move move) {
    int from = move_from(move);
    int to = move_to(move);
    char piece = squares_[from];
    bool white = is_white_piece(piece);
    squares_[from] = '.';

    if (move_is_enpassant(move)) {
        int capture_sq = white ? to - 8 : to + 8;
        squares_[capture_sq] = '.';
    }

    if (move_is_castling(move)) {
        if (piece == 'K' && to == to_index(6, 0)) {
            squares_[to_index(5, 0)] = squares_[to_index(7, 0)];
            squares_[to_index(7, 0)] = '.';
        } else if (piece == 'K' && to == to_index(2, 0)) {
            squares_[to_index(3, 0)] = squares_[to_index(0, 0)];
            squares_[to_index(0, 0)] = '.';
        } else if (piece == 'k' && to == to_index(6, 7)) {
            squares_[to_index(5, 7)] = squares_[to_index(7, 7)];
            squares_[to_index(7, 7)] = '.';
        } else if (piece == 'k' && to == to_index(2, 7)) {
            squares_[to_index(3, 7)] = squares_[to_index(0, 7)];
            squares_[to_index(0, 7)] = '.';
        }
    }

    int promo = move_promo(move);
    if (promo) {
        piece = promotion_from_code(promo, white);
    }

    squares_[to] = piece;

    // Update castling rights
    if (piece == 'K') {
        castling_rights_ &= ~(CASTLE_WHITE_K | CASTLE_WHITE_Q);
    } else if (piece == 'k') {
        castling_rights_ &= ~(CASTLE_BLACK_K | CASTLE_BLACK_Q);
    }

    if (from == to_index(0, 0) || to == to_index(0, 0)) {
        castling_rights_ &= ~CASTLE_WHITE_Q;
    }
    if (from == to_index(7, 0) || to == to_index(7, 0)) {
        castling_rights_ &= ~CASTLE_WHITE_K;
    }
    if (from == to_index(0, 7) || to == to_index(0, 7)) {
        castling_rights_ &= ~CASTLE_BLACK_Q;
    }
    if (from == to_index(7, 7) || to == to_index(7, 7)) {
        castling_rights_ &= ~CASTLE_BLACK_K;
    }

    en_passant_square_ = -1;
    if (std::tolower(static_cast<unsigned char>(piece)) == 'p' && move_is_double_pawn(move)) {
        en_passant_square_ = white ? to - 8 : to + 8;
    }

    stm_white_ = !stm_white_;
}

} // namespace engine
