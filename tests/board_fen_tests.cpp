#include "engine/core/board.hpp"
#include "engine/core/fen.hpp"

#include <iostream>
#include <string>

int main() {
    engine::Board board;
    if (!board.set_fen(std::string(engine::fen::kStartposFEN))) {
        std::cerr << "Failed to load startpos FEN" << std::endl;
        return 1;
    }

    if (!board.white_to_move()) {
        std::cerr << "Expected white to move after loading startpos" << std::endl;
        return 2;
    }

    if (board.set_fen("invalid")) {
        std::cerr << "Invalid FEN should not be accepted" << std::endl;
        return 3;
    }

    return 0;
}
