#include <cstdlib>
#include <iostream>
#include <string>

#include "sirio/board.hpp"
#include "sirio/nnue/features.hpp"

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "usage: sirio_feature_dump \"<fen>\" [\"<fen>\" ...]\n";
        return 2;
    }

    for (int i = 1; i < argc; ++i) {
        const std::string fen = argv[i];
        sirio::Board board;
        board.set_from_fen(fen);

        sirio::nnue::SparseFeatureState state;
        if (!sirio::nnue::encode_sirio_halfka_v1(board, state)) {
            std::cerr << "encode failed for fen: " << fen << "\n";
            return 1;
        }

        std::cout << "FEN=" << fen << "\n";
        for (std::size_t perspective = 0; perspective < sirio::nnue::kSirioHalfKAv1PerspectiveCount; ++perspective) {
            const auto &ps = state.perspectives[perspective];
            std::cout << "P" << perspective << " count=" << ps.count;
            for (std::size_t j = 0; j < ps.count; ++j) {
                std::cout << ' ' << ps.active[j].index << ':' << static_cast<int>(ps.active[j].value);
            }
            std::cout << "\n";
        }
    }

    return 0;
}
