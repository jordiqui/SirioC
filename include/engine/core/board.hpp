#pragma once
#include <array>
#include <string>
#include <vector>
#include "engine/types.hpp"

namespace engine {

class Board {
public:
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
    std::string last_fen_;
    std::array<char, 64> squares_{};
    uint8_t castling_rights_ = 0;
    int en_passant_square_ = -1;
};

} // namespace engine
