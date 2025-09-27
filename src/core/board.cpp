#include "engine/core/board.hpp"
#include "engine/core/fen.hpp"
#include <sstream>

namespace engine {

Board::Board() { set_startpos(); }

void Board::set_startpos() {
    last_fen_ = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    stm_white_ = true;
}

bool Board::set_fen(const std::string& fen) {
    if (!fen::is_valid_fen(fen)) return false;
    last_fen_ = fen;
    std::istringstream iss(fen);
    std::string board_part, stm_field;
    if (iss >> board_part >> stm_field) {
        stm_white_ = (stm_field == "w");
    }
    return true;
}

bool Board::apply_moves_uci(const std::vector<std::string>& /*uci_moves*/) {
    // TODO: translate UCI strings to Move, call make_move
    return true;
}

} // namespace engine
