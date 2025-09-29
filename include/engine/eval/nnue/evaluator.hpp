#pragma once

#include <memory>
#include <string>

namespace engine { class Board; }

namespace nnue { class Network; }

namespace engine::nnue {

class Evaluator {
public:
    Evaluator();
    ~Evaluator();

    bool load_network(const std::string& path);
    int eval_cp(const engine::Board& board) const;
    bool loaded() const noexcept { return static_cast<bool>(network_); }
    const std::string& loaded_path() const noexcept { return loaded_path_; }

private:
    std::unique_ptr<::nnue::Network> network_;
    std::string loaded_path_{};
};

} // namespace engine::nnue
