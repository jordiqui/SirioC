#include "sirio/history.hpp"

#include <algorithm>

namespace sirio {

namespace {

constexpr std::size_t color_to_index(Color color) {
    return color == Color::White ? 0u : 1u;
}

bool moves_match(const Move &lhs, const Move &rhs) {
    return lhs.from == rhs.from && lhs.to == rhs.to && lhs.piece == rhs.piece &&
           lhs.captured == rhs.captured && lhs.promotion == rhs.promotion &&
           lhs.is_en_passant == rhs.is_en_passant && lhs.is_castling == rhs.is_castling;
}

}  // namespace

bool is_quiet_move(const Move &move) {
    return !move.captured.has_value() && !move.promotion.has_value() && !move.is_castling &&
           !move.is_en_passant;
}

int SearchHistory::quiet_history_score(const Move &move, Color mover) const {
    if (!is_quiet_move(move)) {
        return 0;
    }
    const auto idx = color_to_index(mover);
    return quiet_history_[idx][move.from][move.to];
}

void SearchHistory::update_quiet_history(Color mover, const Move &move, int depth, bool success) {
    if (!is_quiet_move(move)) {
        return;
    }
    depth = std::max(depth, 1);
    int bonus = depth * depth;
    bonus = std::min(bonus, search_params::history_bonus_limit);
    auto &entry = quiet_history_[color_to_index(mover)][move.from][move.to];
    if (success) {
        entry = std::min(entry + bonus, search_params::history_max);
    } else {
        entry = std::max(entry - bonus, search_params::history_min);
    }
}

void SearchHistory::store_killer(const Move &move, int ply) {
    if (!is_quiet_move(move) || ply >= search_params::max_search_depth) {
        return;
    }
    auto &slots = killer_moves_[static_cast<std::size_t>(ply)];
    if (!slots[0].has_value() || !moves_match(*slots[0], move)) {
        slots[1] = slots[0];
        slots[0] = move;
    }
}

const std::array<std::optional<Move>, 2> &SearchHistory::killer_slots(int ply) const {
    static const std::array<std::optional<Move>, 2> empty{};
    if (ply < 0 || ply >= search_params::max_search_depth) {
        return empty;
    }
    return killer_moves_[static_cast<std::size_t>(ply)];
}

}  // namespace sirio
