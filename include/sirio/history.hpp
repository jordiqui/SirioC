#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>

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

enum class CaptureNoisyRuntimeUpdateSite {
    MainNegamaxTacticalBetaCutoff,
    MainNegamaxQuietBetaCutoff,
    QuiescenceTacticalBetaCutoff,
    FailedTacticalMove,
};

struct CaptureNoisyRuntimeUpdateCounters {
    int applied = 0;
};

enum class ContinuationRuntimeUpdateSite {
    MainNegamaxQuietBetaCutoff,
    MainNegamaxCaptureBetaCutoff,
    MainNegamaxPromotionBetaCutoff,
    MainNegamaxQuietNonCutoff,
    QuiescenceQuietBetaCutoff,
};

struct ContinuationRuntimeUpdateCounters {
    int quiet_beta_cutoff_applied = 0;
    int quiet_beta_cutoff_malus_applied = 0;
    int quiet_beta_cutoff_skipped = 0;
};
struct CorrectionRuntimeUpdateCounters {
    int quiet_beta_cutoff_applied = 0;
    int fail_low_applied = 0;
};
struct ReverseFutilityRuntimeCounters {
    int return_applied = 0;
};
struct MoveCountPruningRuntimeCounters {
    int continue_applied = 0;
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
        [[nodiscard]] int score(const CorrectionHistoryKey &key) const;
        [[nodiscard]] int score(Color mover, std::size_t bucket) const;
        void update(const CorrectionHistoryKey &key, int bonus);
        void update(Color mover, std::size_t bucket, int depth, bool success);
        void clear();

    private:
        static constexpr std::size_t bucket_count_ = 1024;
        [[nodiscard]] static bool is_valid_key(const CorrectionHistoryKey &key);
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
    [[nodiscard]] const CaptureNoisyRuntimeUpdateCounters &capture_noisy_runtime_update_counters() const {
        return capture_noisy_runtime_update_counters_;
    }
    void reset_capture_noisy_runtime_update_counters();
    void record_capture_noisy_runtime_update_applied();
    [[nodiscard]] int continuation_quiet_beta_cutoff_update_count_for_tests() const;
    [[nodiscard]] int continuation_quiet_beta_cutoff_malus_count_for_tests() const;
    [[nodiscard]] int continuation_quiet_beta_cutoff_skip_count_for_tests() const;
    void reset_continuation_runtime_observability_for_tests();
    void record_continuation_quiet_beta_cutoff_update_for_tests();
    void record_continuation_quiet_beta_cutoff_malus_for_tests();
    void record_continuation_quiet_beta_cutoff_skip_for_tests();
    [[nodiscard]] int correction_quiet_beta_cutoff_update_count_for_tests() const;
    [[nodiscard]] int correction_fail_low_update_count_for_tests() const;
    void reset_correction_runtime_observability_for_tests();
    void record_correction_quiet_beta_cutoff_update_for_tests();
    void record_correction_fail_low_update_for_tests();
    [[nodiscard]] int reverse_futility_return_count_for_tests() const;
    void record_reverse_futility_return();
    void reset_reverse_futility_runtime_observability_for_tests();
    [[nodiscard]] int move_count_pruning_continue_count_for_tests() const;
    void record_move_count_pruning_continue();
    void reset_move_count_pruning_runtime_observability_for_tests();

private:
    std::array<std::array<std::optional<Move>, 2>, search_params::max_search_depth> killer_moves_{};
    std::array<std::array<std::array<int, 64>, 64>, 2> quiet_history_{};
    CaptureHistory capture_history_{};
    NoisyHistory noisy_history_{};
    ContinuationHistory continuation_history_{};
    CorrectionHistory correction_history_{};
    CaptureNoisyRuntimeUpdateCounters capture_noisy_runtime_update_counters_{};
    ContinuationRuntimeUpdateCounters continuation_runtime_update_counters_{};
    CorrectionRuntimeUpdateCounters correction_runtime_update_counters_{};
    ReverseFutilityRuntimeCounters reverse_futility_runtime_counters_{};
    MoveCountPruningRuntimeCounters move_count_pruning_runtime_counters_{};
};

[[nodiscard]] bool is_quiet_move(const Move &move);
[[nodiscard]] std::optional<CaptureHistoryKey> make_capture_history_key(const Board &board, const Move &move);
[[nodiscard]] std::optional<NoisyHistoryKey> make_noisy_history_key(const Board &board, const Move &move);

[[nodiscard]] std::optional<ContinuationHistoryKey> make_continuation_history_key(
    const Board &previous_board, const std::optional<Move> &previous_move, const Board &current_board,
    const Move &current_move);
[[nodiscard]] std::optional<CorrectionHistoryKey> make_correction_history_key(Color mover_color, std::size_t bucket);
[[nodiscard]] std::optional<CorrectionHistoryKey> make_correction_history_key_from_position(const Board &board);
[[nodiscard]] int apply_correction_history_to_static_eval(
    int raw_static_eval, const SearchHistory::CorrectionHistory &correction_history,
    const std::optional<CorrectionHistoryKey> &key);
[[nodiscard]] int apply_correction_history_to_static_eval(
    int raw_static_eval, const SearchHistory &history, const std::optional<CorrectionHistoryKey> &key);
[[nodiscard]] CaptureNoisyHistoryUpdate make_capture_noisy_history_update(
    const std::optional<CaptureHistoryKey> &capture_key, const std::optional<NoisyHistoryKey> &noisy_key,
    bool success, int depth);
void apply_capture_noisy_history_update(SearchHistory &history, const CaptureNoisyHistoryUpdate &update);
void apply_capture_noisy_history_update_for_tests(SearchHistory &history, const CaptureNoisyHistoryUpdate &update);
bool apply_capture_noisy_runtime_update_for_tests(SearchHistory &history, CaptureNoisyRuntimeUpdateSite site,
                                                  const std::optional<CaptureHistoryKey> &capture_key,
                                                  const std::optional<NoisyHistoryKey> &noisy_key, int depth);
bool apply_continuation_runtime_update_for_tests(
    SearchHistory &history, ContinuationRuntimeUpdateSite site,
    const std::optional<ContinuationHistoryKey> &continuation_key,
    const std::span<const ContinuationHistoryKey> &tried_quiet_keys, int depth);
bool apply_correction_history_quiet_beta_cutoff_update(
    SearchHistory &history, const std::optional<CorrectionHistoryKey> &correction_key, int raw_static_eval,
    int cutoff_value);
bool apply_correction_history_fail_low_update(
    SearchHistory &history, const std::optional<CorrectionHistoryKey> &correction_key, int raw_static_eval,
    int best_value);
bool apply_correction_history_quiet_beta_cutoff_update_for_tests(
    SearchHistory &history, const std::optional<CorrectionHistoryKey> &correction_key, int raw_static_eval,
    int cutoff_value);
bool apply_correction_history_fail_low_update_for_tests(
    SearchHistory &history, const std::optional<CorrectionHistoryKey> &correction_key, int raw_static_eval,
    int best_value);

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

[[nodiscard]] inline std::optional<CorrectionHistoryKey> make_correction_history_key_from_position_for_tests(
    const Board &board) {
    return make_correction_history_key_from_position(board);
}

}  // namespace sirio
