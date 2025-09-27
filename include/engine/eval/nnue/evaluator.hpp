#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace engine { class Board; }

namespace engine::nnue {

class Evaluator {
public:
    bool load_network(const std::string& path);
    int eval_cp(const engine::Board& board) const;
    bool loaded() const noexcept { return loaded_; }

private:
    bool loaded_ = false;
    std::vector<std::uint8_t> raw_network_;
};

} // namespace engine::nnue

