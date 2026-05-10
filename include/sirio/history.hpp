#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include "sirio/move.hpp"
#include "sirio/search_params.hpp"

namespace sirio {

struct CaptureHistoryKey {
    Color mover = Color::White;
    PieceType attacker = PieceType::Pawn;
    PieceType captured = PieceType::Pawn;
    int to = 0;
};

struct NoisyHistoryKey {
    Color mover = Color::White;
    PieceType mover_piece = PieceType::Pawn;
    int to = 0;
};

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

    class CorrectionHistory {
    public:
        [[nodiscard]] int score(Color mover, std::size_t bucket) const;
        void update(Color mover, std::size_t bucket, int depth, bool success);
        void clear();

    private:
        static constexpr std::size_t bucket_count_ = 1024;
        [[nodiscard]] static std::size_t normalize_bucket(std::size_t bucket);
        std::array<std::array<int, bucket_count_>, 2> table_{};
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
    [[nodiscard]] const CorrectionHistory &correction_history() const { return correction_history_; }
    [[nodiscard]] CorrectionHistory &correction_history() { return correction_history_; }

private:
    std::array<std::array<std::optional<Move>, 2>, search_params::max_search_depth> killer_moves_{};
    std::array<std::array<std::array<int, 64>, 64>, 2> quiet_history_{};
    CaptureHistory capture_history_{};
    NoisyHistory noisy_history_{};
    ContinuationHistory continuation_history_{};
    CorrectionHistory correction_history_{};
};

[[nodiscard]] bool is_quiet_move(const Move &move);
[[nodiscard]] std::optional<CaptureHistoryKey> make_capture_history_key(const Board &board, const Move &move);
[[nodiscard]] std::optional<NoisyHistoryKey> make_noisy_history_key(const Board &board, const Move &move);

[[nodiscard]] inline std::optional<CaptureHistoryKey> make_capture_history_key_for_tests(const Board &board,
                                                                                          const Move &move) {
    return make_capture_history_key(board, move);
}

[[nodiscard]] inline std::optional<NoisyHistoryKey> make_noisy_history_key_for_tests(const Board &board,
                                                                                       const Move &move) {
    return make_noisy_history_key(board, move);
}

}  // namespace sirio
