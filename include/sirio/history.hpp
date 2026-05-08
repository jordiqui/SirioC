#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include "sirio/move.hpp"
#include "sirio/search_params.hpp"

namespace sirio {

class SearchHistory {
public:
    [[nodiscard]] int quiet_history_score(const Move &move, Color mover) const;
    void update_quiet_history(Color mover, const Move &move, int depth, bool success);
    void store_killer(const Move &move, int ply);

    [[nodiscard]] const std::array<std::optional<Move>, 2> &killer_slots(int ply) const;

private:
    std::array<std::array<std::optional<Move>, 2>, search_params::max_search_depth> killer_moves_{};
    std::array<std::array<std::array<int, 64>, 64>, 2> quiet_history_{};
};

[[nodiscard]] bool is_quiet_move(const Move &move);

}  // namespace sirio
