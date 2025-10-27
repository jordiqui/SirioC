#include "sirio/move.hpp"

#include <cctype>
#include <cstdlib>
#include <stdexcept>

#include "sirio/bitboard.hpp"
#include "sirio/board.hpp"
#include "sirio/movegen.hpp"

namespace sirio {

namespace {
char promotion_char(PieceType piece) {
    switch (piece) {
        case PieceType::Queen:
            return 'q';
        case PieceType::Rook:
            return 'r';
        case PieceType::Bishop:
            return 'b';
        case PieceType::Knight:
            return 'n';
        default:
            throw std::invalid_argument("Invalid promotion piece");
    }
}

PieceType piece_from_promotion_char(char symbol) {
    switch (symbol) {
        case 'q':
            return PieceType::Queen;
        case 'r':
            return PieceType::Rook;
        case 'b':
            return PieceType::Bishop;
        case 'n':
            return PieceType::Knight;
        default:
            throw std::invalid_argument("Unknown promotion piece");
    }
}

int square_from_uci(const std::string &text, std::size_t offset) {
    if (offset + 1 >= text.size()) {
        throw std::invalid_argument("Invalid UCI move format");
    }
    char file = text[offset];
    char rank = text[offset + 1];
    if (file < 'a' || file > 'h' || rank < '1' || rank > '8') {
        throw std::invalid_argument("Invalid square in UCI move");
    }
    return (rank - '1') * 8 + (file - 'a');
}

}  // namespace

std::string move_to_uci(const Move &move) {
    std::string result;
    result.reserve(5);
    result.push_back(static_cast<char>('a' + file_of(move.from)));
    result.push_back(static_cast<char>('1' + rank_of(move.from)));
    result.push_back(static_cast<char>('a' + file_of(move.to)));
    result.push_back(static_cast<char>('1' + rank_of(move.to)));
    if (move.promotion) {
        result.push_back(promotion_char(*move.promotion));
    }
    return result;
}

Move move_from_uci(const Board &board, const std::string &uci) {
    if (uci.size() < 4) {
        throw std::invalid_argument("UCI move too short");
    }

    const int from = square_from_uci(uci, 0);
    const int to = square_from_uci(uci, 2);

    std::optional<PieceType> promotion;
    if (uci.size() == 5) {
        const unsigned char promotion_char_raw = static_cast<unsigned char>(uci[4]);
        const char normalized_promotion = static_cast<char>(std::tolower(promotion_char_raw));
        promotion = piece_from_promotion_char(normalized_promotion);
    } else if (uci.size() != 4) {
        throw std::invalid_argument("Invalid UCI move length");
    }

    auto legal_moves = generate_legal_moves(board);
    for (const Move &move : legal_moves) {
        if (move.from != from || move.to != to) {
            continue;
        }
        if (promotion.has_value() != move.promotion.has_value()) {
            continue;
        }
        if (promotion && move.promotion && *promotion != *move.promotion) {
            continue;
        }
        return move;
    }

    throw std::invalid_argument("UCI move is not legal in the current position");
}

bool apply_uci_move(Board &board, const std::string &uci_token) {
    if (uci_token == "0000") {
        board = board.apply_null_move();
        return true;
    }

    try {
        Move move = move_from_uci(board, uci_token);
        board = board.apply_move(move);
        return true;
    } catch (const std::exception &) {
        return false;
    }
}

bool validate_move(const Board &board, const Move &move, Board *next_board) {
    const Color us = board.side_to_move();
    const Color them = opposite(us);

    Board next;
    try {
        next = board.apply_move(move);
    } catch (const std::exception &) {
        return false;
    }

    const int king_sq = next.king_square(us);
    if (king_sq >= 0 && next.is_square_attacked(king_sq, them)) {
        return false;
    }

    const CastlingRights before = board.castling_rights();
    const CastlingRights after = next.castling_rights();

    auto rook_start_square = [](Color color, bool kingside) {
        if (color == Color::White) {
            return kingside ? 7 : 0;
        }
        return kingside ? 63 : 56;
    };

    auto castling_change_valid = [&](Color color, bool kingside, bool before_right, bool after_right) {
        if (after_right && !before_right) {
            return false;
        }
        if (before_right && !after_right) {
            if (color == us) {
                if (move.piece == PieceType::King) {
                    return true;
                }
                if (move.piece == PieceType::Rook && move.from == rook_start_square(color, kingside)) {
                    return true;
                }
            } else {
                if (move.captured.has_value() && *move.captured == PieceType::Rook &&
                    move.to == rook_start_square(color, kingside)) {
                    return true;
                }
            }
            return false;
        }
        return true;
    };

    if (!castling_change_valid(Color::White, true, before.white_kingside, after.white_kingside) ||
        !castling_change_valid(Color::White, false, before.white_queenside, after.white_queenside) ||
        !castling_change_valid(Color::Black, true, before.black_kingside, after.black_kingside) ||
        !castling_change_valid(Color::Black, false, before.black_queenside, after.black_queenside)) {
        return false;
    }

    auto after_ep = next.en_passant_square();
    if (move.piece == PieceType::Pawn && std::abs(move.to - move.from) == 16) {
        int expected = move.to > move.from ? move.from + 8 : move.from - 8;
        if (!after_ep.has_value() || *after_ep != expected) {
            return false;
        }
    } else if (after_ep.has_value()) {
        return false;
    }

    if (next_board != nullptr) {
        *next_board = std::move(next);
    }

    return true;
}

}  // namespace sirio

