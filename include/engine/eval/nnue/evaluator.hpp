#pragma once
#include <string>

namespace engine { class Board; }

namespace engine::nnue {

class Evaluator {
public:
    bool load_network(const std::string& path) {
        // TODO: parse .nnue header and weights
        (void)path; return true;
    }

    int eval_cp(const engine::Board&) const {
        // TODO: integer-domain inference
        return 0;
    }
};

} // namespace engine::nnue
