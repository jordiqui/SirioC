#include <iostream>
#include <string>
#include "engine/core/board.hpp"

int main() {
    std::string fen;
    if (!std::getline(std::cin, fen)) {
        return 1;
    }
    engine::Board board;
    if (!board.set_fen(fen)) {
        std::cerr << "invalid fen";
        return 2;
    }
    auto moves = board.generate_legal_moves();
    for (size_t i = 0; i < moves.size(); ++i) {
        if (i) std::cout << ' ';
        std::cout << board.move_to_uci(moves[i]);
    }
    std::cout << '\n';
    return 0;
}
