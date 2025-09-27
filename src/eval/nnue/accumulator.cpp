#include "engine/eval/nnue/accumulator.hpp"

#include <cctype>

#include "engine/core/board.hpp"
#include "engine/eval/nnue/psqt.hpp"

namespace engine::nnue {

namespace {
inline void apply_piece(int& mg, int& eg, int& phase, char piece, int sq, int delta) {
    int idx = piece_index(piece);
    if (idx < 0) return;
    bool white = std::isupper(static_cast<unsigned char>(piece));
    int table_sq = white ? sq : mirror_square(sq);
    int mg_val = kMgPieceValues[static_cast<size_t>(idx)] +
                 kMgPst[static_cast<size_t>(idx)][static_cast<size_t>(table_sq)];
    int eg_val = kEgPieceValues[static_cast<size_t>(idx)] +
                 kEgPst[static_cast<size_t>(idx)][static_cast<size_t>(table_sq)];
    if (white) {
        mg += delta * mg_val;
        eg += delta * eg_val;
    } else {
        mg -= delta * mg_val;
        eg -= delta * eg_val;
    }
    phase += delta * kGamePhaseInc[static_cast<size_t>(idx)];
}
} // namespace

void Accumulator::reset(const Board& board) {
    mg_score_ = 0;
    eg_score_ = 0;
    phase_ = 0;
    const auto& squares = board.squares();
    for (int sq = 0; sq < 64; ++sq) {
        add_piece(squares[static_cast<size_t>(sq)], sq);
    }
}

void Accumulator::add_piece(char piece, int sq) noexcept {
    apply_piece(mg_score_, eg_score_, phase_, piece, sq, +1);
}

void Accumulator::remove_piece(char piece, int sq) noexcept {
    apply_piece(mg_score_, eg_score_, phase_, piece, sq, -1);
}

int Accumulator::evaluate(bool stm_white) const noexcept {
    int phase = std::clamp(phase_, 0, kGamePhaseMax);
    int eg_phase = kGamePhaseMax - phase;
    int score = (mg_score_ * phase + eg_score_ * eg_phase) / kGamePhaseMax;
    score += stm_white ? 10 : -10;
    return stm_white ? score : -score;
}

} // namespace engine::nnue

