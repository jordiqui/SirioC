#pragma once

#include "pyrrhic/board.h"

#include <string>

extern "C" {
#include "nn/evaluate.h"
}

namespace sirio::nn {

class Evaluator {
public:
    Evaluator();

    int evaluate(const pyrrhic::Board& board) const;
    bool load_network(const std::string& path);
    void set_piece_value(pyrrhic::PieceType type, int value);
    int piece_value(pyrrhic::PieceType type) const;

private:
    sirio_nn_model model_;
};

}  // namespace sirio::nn
