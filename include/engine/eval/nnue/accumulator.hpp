#pragma once

#include <algorithm>
#include <cstdint>

namespace engine {
class Board;
}

namespace engine::nnue {

class Accumulator {
public:
    void reset(const Board& board);

    void add_piece(char piece, int sq) noexcept;
    void remove_piece(char piece, int sq) noexcept;

    int mg() const noexcept { return mg_score_; }
    int eg() const noexcept { return eg_score_; }
    int phase() const noexcept { return phase_; }

    void restore(int mg_score, int eg_score, int phase) noexcept {
        mg_score_ = mg_score;
        eg_score_ = eg_score;
        phase_ = phase;
    }

    int evaluate(bool stm_white) const noexcept;

private:
    int mg_score_ = 0;
    int eg_score_ = 0;
    int phase_ = 0;
};

} // namespace engine::nnue

