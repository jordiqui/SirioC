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

struct ContinuationHistoryKey {
    Color previous_mover_color = Color::White;
    Color current_mover_color = Color::White;
    PieceType previous_moving_piece = PieceType::Pawn;
    int previous_to_square = 0;
    PieceType current_moving_piece = PieceType::Pawn;
    int current_to_square = 0;
};

struct CorrectionHistoryKey {
    Color mover_color = Color::White;
    std::size_t bucket = 0;
};

enum class CaptureNoisyHistoryUpdateTarget {
    None,
    Capture,
    Noisy,
};


struct CaptureNoisyHistoryUpdateEvent {
    CaptureNoisyHistoryUpdateTarget target = CaptureNoisyHistoryUpdateTarget::None;
    std::optional<CaptureHistoryKey> capture_key;
    std::optional<NoisyHistoryKey> noisy_key;
    int depth = 0;
    bool success = false;
    const char *reason = "";
};
struct CaptureNoisyHistoryUpdate {
    CaptureNoisyHistoryUpdateTarget target = CaptureNoisyHistoryUpdateTarget::None;
    bool success = false;
    int bonus = 0;
    std::optional<CaptureHistoryKey> capture_key;
    std::optional<NoisyHistoryKey> noisy_key;
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

[[nodiscard]] std::optional<ContinuationHistoryKey> make_continuation_history_key(
    const Board &previous_board, const std::optional<Move> &previous_move, const Board &current_board,
    const Move &current_move);
[[nodiscard]] std::optional<CorrectionHistoryKey> make_correction_history_key(Color mover_color, std::size_t bucket);
[[nodiscard]] CaptureNoisyHistoryUpdate make_capture_noisy_history_update(
    const std::optional<CaptureHistoryKey> &capture_key, const std::optional<NoisyHistoryKey> &noisy_key,
    bool success, int depth);
void apply_capture_noisy_history_update(SearchHistory &history, const CaptureNoisyHistoryUpdate &update);
void apply_capture_noisy_history_update_for_tests(SearchHistory &history, const CaptureNoisyHistoryUpdate &update);

[[nodiscard]] CaptureNoisyHistoryUpdateEvent make_capture_noisy_history_update_event_for_tests(
    CaptureNoisyHistoryUpdateTarget target, const std::optional<CaptureHistoryKey> &capture_key,
    const std::optional<NoisyHistoryKey> &noisy_key, int depth, bool success, const char *reason = "");
void apply_capture_noisy_history_update_event_for_tests(SearchHistory &history,
                                                        const CaptureNoisyHistoryUpdateEvent &event);

[[nodiscard]] inline std::optional<CaptureHistoryKey> make_capture_history_key_for_tests(const Board &board,
                                                                                          const Move &move) {
    return make_capture_history_key(board, move);
}

[[nodiscard]] inline std::optional<NoisyHistoryKey> make_noisy_history_key_for_tests(const Board &board,
                                                                                       const Move &move) {
    return make_noisy_history_key(board, move);
}

[[nodiscard]] inline std::optional<ContinuationHistoryKey> make_continuation_history_key_for_tests(
    const Board &previous_board, const std::optional<Move> &previous_move, const Board &current_board,
    const Move &current_move) {
    return make_continuation_history_key(previous_board, previous_move, current_board, current_move);
}

[[nodiscard]] inline std::optional<CorrectionHistoryKey> make_correction_history_key_for_tests(
    Color mover_color, std::size_t bucket) {
    return make_correction_history_key(mover_color, bucket);
}

}  // namespace sirio
