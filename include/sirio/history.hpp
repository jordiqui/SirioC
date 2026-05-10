#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include "sirio/move.hpp"
#include "sirio/search_params.hpp"

namespace sirio {

class SearchHistory {
public:
    class CaptureHistory {
    public:
        [[nodiscard]] int score(const Move &move, Color mover) const;
        void update(Color mover, const Move &move, int depth, bool success);
        void clear();

    private:
        std::array<std::array<std::array<std::array<int, 64>, static_cast<std::size_t>(PieceType::Count)>,
                              static_cast<std::size_t>(PieceType::Count)>,
                   2>
            table_{};
    };

    class NoisyHistory {
    public:
        [[nodiscard]] int score(const Move &move, Color mover) const;
        void update(Color mover, const Move &move, int depth, bool success);
        void clear();

    private:
        std::array<std::array<std::array<int, 64>, static_cast<std::size_t>(PieceType::Count)>, 2> table_{};
    };

    class ContinuationHistory {
    public:
        [[nodiscard]] int score(Color previous_mover, const Move &previous_move, Color current_mover,
                                const Move &current_move) const;
        void update(Color previous_mover, const Move &previous_move, Color current_mover, const Move &current_move,
                    int depth, bool success);
        void clear();

    private:
        std::array<
            std::array<
                std::array<std::array<std::array<std::array<int, 64>, static_cast<std::size_t>(PieceType::Count)>,
                                      64>,
                           static_cast<std::size_t>(PieceType::Count)>,
                2>,
            2>
            table_{};
    };

    [[nodiscard]] int quiet_history_score(const Move &move, Color mover) const;
    void update_quiet_history(Color mover, const Move &move, int depth, bool success);
    void store_killer(const Move &move, int ply);
    void clear();

    [[nodiscard]] const std::array<std::optional<Move>, 2> &killer_slots(int ply) const;
    [[nodiscard]] const CaptureHistory &capture_history() const { return capture_history_; }
    [[nodiscard]] CaptureHistory &capture_history() { return capture_history_; }
    [[nodiscard]] const NoisyHistory &noisy_history() const { return noisy_history_; }
    [[nodiscard]] NoisyHistory &noisy_history() { return noisy_history_; }
    [[nodiscard]] const ContinuationHistory &continuation_history() const { return continuation_history_; }
    [[nodiscard]] ContinuationHistory &continuation_history() { return continuation_history_; }

private:
    std::array<std::array<std::optional<Move>, 2>, search_params::max_search_depth> killer_moves_{};
    std::array<std::array<std::array<int, 64>, 64>, 2> quiet_history_{};
    CaptureHistory capture_history_{};
    NoisyHistory noisy_history_{};
    ContinuationHistory continuation_history_{};
};

[[nodiscard]] bool is_quiet_move(const Move &move);

}  // namespace sirio
