#pragma once
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

    // TODO: move generation and make/unmake hooks
    // bool make_move(Move m);
    // void unmake_move();

    // Accessors
    bool white_to_move() const { return stm_white_; }
    const std::string& last_fen() const { return last_fen_; }

private:
    bool stm_white_ = true;
    std::string last_fen_;
};

} // namespace engine
