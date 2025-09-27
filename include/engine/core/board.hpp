#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "engine/types.hpp"

namespace engine {

class Board {
public:
    enum PieceIndex : int {
        WHITE_PAWN = 0,
        WHITE_KNIGHT,
        WHITE_BISHOP,
        WHITE_ROOK,
        WHITE_QUEEN,
        WHITE_KING,
        BLACK_PAWN,
        BLACK_KNIGHT,
        BLACK_BISHOP,
        BLACK_ROOK,
        BLACK_QUEEN,
        BLACK_KING,
        PIECE_NB
    };

    enum OccupancyIndex : int {
        OCC_WHITE = 0,
        OCC_BLACK,
        OCC_BOTH,
        OCC_NB
    };

    enum CastlingRight : uint8_t {
        CASTLE_WHITE_KINGSIDE  = 1u << 0,
        CASTLE_WHITE_QUEENSIDE = 1u << 1,
        CASTLE_BLACK_KINGSIDE  = 1u << 2,
        CASTLE_BLACK_QUEENSIDE = 1u << 3
    };

    static constexpr int INVALID_SQUARE = -1;
    static constexpr std::string_view kStartposFEN =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    Board();
    void set_startpos();
    bool set_fen(const std::string& fen);
    bool apply_moves_uci(const std::vector<std::string>& uci_moves);

    bool make_move(Move m);
    bool make_move_uci(const std::string& uci);
    std::vector<Move> generate_legal_moves() const;
    std::string move_to_uci(Move move) const;

    // Accessors
    bool white_to_move() const { return stm_white_; }
    const std::string& last_fen() const { return last_fen_; }
    const std::array<uint64_t, PIECE_NB>& piece_bitboards() const { return piece_bitboards_; }
    const std::array<uint64_t, OCC_NB>& occupancy() const { return occupancy_; }
    char piece_on(int square) const { return squares_[square]; }
    uint8_t castling_rights() const { return castling_rights_; }
    int en_passant_square() const { return en_passant_square_; }
    int halfmove_clock() const { return halfmove_clock_; }
    int fullmove_number() const { return fullmove_number_; }

private:
    enum Castling : uint8_t {
        CASTLE_WHITE_K = 1 << 0,
        CASTLE_WHITE_Q = 1 << 1,
        CASTLE_BLACK_K = 1 << 2,
        CASTLE_BLACK_Q = 1 << 3,
    };

    void generate_pseudo_legal_moves(std::vector<Move>& moves) const;
    void generate_pawn_moves(int sq, std::vector<Move>& moves) const;
    void generate_knight_moves(int sq, std::vector<Move>& moves) const;
    void generate_bishop_moves(int sq, std::vector<Move>& moves) const;
    void generate_rook_moves(int sq, std::vector<Move>& moves) const;
    void generate_queen_moves(int sq, std::vector<Move>& moves) const;
    void generate_king_moves(int sq, std::vector<Move>& moves) const;
    bool in_check(bool white) const;
    bool is_square_attacked(int sq, bool by_white) const;
    int find_king_square(bool white) const;
    void do_move(Move move);
    static bool is_white_piece(char piece);
    static bool is_black_piece(char piece);
    static bool is_empty(char piece);
    static int file_of(int sq);
    static int rank_of(int sq);
    static int to_index(int file, int rank);
    std::string square_to_string(int sq) const;
    char promotion_from_code(int code, bool white) const;

    bool stm_white_ = true;
    std::array<uint64_t, PIECE_NB> piece_bitboards_{};
    std::array<uint64_t, OCC_NB> occupancy_{};
    uint8_t castling_rights_ = 0;
    int en_passant_square_ = INVALID_SQUARE;
    int halfmove_clock_ = 0;
    int fullmove_number_ = 1;
    std::string last_fen_;
    std::array<char, 64> squares_{};
};

} // namespace engine
