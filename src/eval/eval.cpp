#include "engine/eval/eval.hpp"

#include <algorithm>
#include <cctype>

#include "engine/eval/nnue/psqt.hpp"

namespace engine::eval {

int evaluate(const Board& board) {
    int mg_score = 0;
    int eg_score = 0;
    int phase = 0;

    const auto& squares = board.squares();
    for (int sq = 0; sq < 64; ++sq) {
        char piece = squares[static_cast<size_t>(sq)];
        if (piece == '.') continue;
        int idx = nnue::piece_index(piece);
        if (idx < 0) continue;
        bool white = std::isupper(static_cast<unsigned char>(piece));
        int table_sq = white ? sq : nnue::mirror_square(sq);
        int mg = nnue::kMgPieceValues[static_cast<size_t>(idx)] +
                 nnue::kMgPst[static_cast<size_t>(idx)][static_cast<size_t>(table_sq)];
        int eg = nnue::kEgPieceValues[static_cast<size_t>(idx)] +
                 nnue::kEgPst[static_cast<size_t>(idx)][static_cast<size_t>(table_sq)];
        if (white) {
            mg_score += mg;
            eg_score += eg;
        } else {
            mg_score -= mg;
            eg_score -= eg;
        }
        phase += nnue::kGamePhaseInc[static_cast<size_t>(idx)];
    }

    phase = std::clamp(phase, 0, nnue::kGamePhaseMax);
    int eg_phase = nnue::kGamePhaseMax - phase;
    int score = (mg_score * phase + eg_score * eg_phase) / nnue::kGamePhaseMax;
    score += board.white_to_move() ? 10 : -10;
    return board.white_to_move() ? score : -score;
}

} // namespace engine::eval

