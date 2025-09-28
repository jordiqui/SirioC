#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace engine { class Board; }

namespace engine::nnue {

class Evaluator {
public:
    bool load_network(const std::string& path);
    int eval_cp(const engine::Board& board) const;
    bool loaded() const noexcept { return loaded_; }
    const std::string& loaded_path() const noexcept { return loaded_path_; }

private:
    struct Network {
        static constexpr std::size_t kFeatureCount = 7;
        double bias = 0.0;
        std::array<double, kFeatureCount> weights{};
    };

    std::array<double, Network::kFeatureCount>
    compute_features(const engine::Board& board) const;

    bool loaded_ = false;
    Network network_{};
    std::string loaded_path_{};
};

} // namespace engine::nnue

