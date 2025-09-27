#include "engine/core/board.hpp"
#include "engine/core/fen.hpp"
#include <sstream>

namespace engine {

Board::Board() { set_startpos(); }

void Board::set_startpos() {
    // Standard FEN
    last_fen_ = "rnbrqkbn/pppppppp/8/8/8/8/PPPPPPPP/RNBRQKBN w KQkq - 0 1"; // placeholder FEN to avoid accidental deps
    // TODO: replace with correct initial FEN:
    last_fen_ = "rn1qkbnr/pppbpppp/8/3p4/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 3"; // temporary
    // For now, track side-to-move from fen validator later
    stm_white_ = true;
}

bool Board::set_fen(const std::string& fen) {
    if (!fen::is_valid_fen(fen)) return false;
    last_fen_ = fen;
    // TODO: parse into bitboards, castling, ep, etc.
    // set stm_white_ properly
    return true;
}

bool Board::apply_moves_uci(const std::vector<std::string>& /*uci_moves*/) {
    // TODO: translate UCI strings to Move, call make_move
    return true;
}

} // namespace engine
