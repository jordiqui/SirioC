#include "sirio/history.hpp"

#include <algorithm>

namespace sirio {

namespace {

constexpr std::size_t color_to_index(Color color) {
    return color == Color::White ? 0u : 1u;
}

bool is_valid_color(Color color) {
    return color == Color::White || color == Color::Black;
}

bool moves_match(const Move &lhs, const Move &rhs) {
    return lhs.from == rhs.from && lhs.to == rhs.to && lhs.piece == rhs.piece &&
           lhs.captured == rhs.captured && lhs.promotion == rhs.promotion &&
           lhs.is_en_passant == rhs.is_en_passant && lhs.is_castling == rhs.is_castling;
}

constexpr std::size_t piece_to_index(PieceType piece) {
    return static_cast<std::size_t>(piece);
}

int history_bonus_for_depth(int depth) {
    depth = std::max(depth, 1);
    int bonus = depth * depth;
    return std::min(bonus, search_params::history_bonus_limit);
}

int normalize_correction_history_runtime_delta(int raw_delta) {
    if (raw_delta == 0) {
        return 0;
    }
    const int scaled_delta = raw_delta / search_params::correction_history_runtime_delta_scale;
    if (scaled_delta == 0) {
        return 0;
    }
    return std::clamp(scaled_delta, -search_params::correction_history_runtime_delta_max,
                      search_params::correction_history_runtime_delta_max);
}

void apply_history_delta(int &entry, int bonus, bool success) {
    if (success) {
        entry = std::min(entry + bonus, search_params::history_max);
    } else {
        entry = std::max(entry - bonus, search_params::history_min);
    }
}

Move move_from_capture_key(const CaptureHistoryKey &key) {
    Move move{};
    move.to = key.to;
    move.piece = key.attacker;
    move.captured = key.captured;
    return move;
}

Move move_from_noisy_key(const NoisyHistoryKey &key) {
    Move move{};
    move.to = key.to;
    move.piece = key.mover_piece;
    move.promotion = PieceType::Queen;
    return move;
}

}  // namespace

bool is_quiet_move(const Move &move) {
    return !move.captured.has_value() && !move.promotion.has_value() && !move.is_castling &&
           !move.is_en_passant;
}

std::optional<CaptureHistoryKey> make_capture_history_key(const Board &board, const Move &move) {
    const auto mover_piece = board.piece_at(move.from);
    if (!mover_piece.has_value() || mover_piece->first != board.side_to_move()) {
        return std::nullopt;
    }

    if (!move.captured.has_value()) {
        return std::nullopt;
    }

    const int capture_square = move.is_en_passant
                                   ? (board.side_to_move() == Color::White ? move.to - 8 : move.to + 8)
                                   : move.to;
    const auto captured_piece = board.piece_at(capture_square);
    if (!captured_piece.has_value() || captured_piece->first == board.side_to_move()) {
        return std::nullopt;
    }

    if (captured_piece->second != *move.captured) {
        return std::nullopt;
    }

    return CaptureHistoryKey{board.side_to_move(), move.piece, *move.captured, move.to};
}

std::optional<NoisyHistoryKey> make_noisy_history_key(const Board &board, const Move &move) {
    const auto mover_piece = board.piece_at(move.from);
    if (!mover_piece.has_value() || mover_piece->first != board.side_to_move()) {
        return std::nullopt;
    }

    if (is_quiet_move(move)) {
        return std::nullopt;
    }

    return NoisyHistoryKey{board.side_to_move(), move.piece, move.to};
}

std::optional<ContinuationHistoryKey> make_continuation_history_key(
    const Board &previous_board, const std::optional<Move> &previous_move, const Board &current_board,
    const Move &current_move) {
    if (!previous_move.has_value()) {
        return std::nullopt;
    }

    const auto previous_mover_piece = previous_board.piece_at(previous_move->from);
    if (!previous_mover_piece.has_value() || previous_mover_piece->first != previous_board.side_to_move() ||
        previous_mover_piece->second != previous_move->piece || !validate_move(previous_board, *previous_move)) {
        return std::nullopt;
    }

    const auto current_mover_piece = current_board.piece_at(current_move.from);
    if (!current_mover_piece.has_value() || current_mover_piece->first != current_board.side_to_move() ||
        current_mover_piece->second != current_move.piece || !validate_move(current_board, current_move)) {
        return std::nullopt;
    }

    return ContinuationHistoryKey{previous_board.side_to_move(), current_board.side_to_move(), previous_move->piece,
                                  previous_move->to, current_move.piece, current_move.to};
}

std::optional<CorrectionHistoryKey> make_correction_history_key(Color mover_color, std::size_t bucket) {
    if (!is_valid_color(mover_color)) {
        return std::nullopt;
    }
    return CorrectionHistoryKey{mover_color, bucket};
}

std::optional<CorrectionHistoryKey> make_correction_history_key_from_position(const Board &board) {
    const Color mover = board.side_to_move();
    if (!is_valid_color(mover)) {
        return std::nullopt;
    }

    const std::uint64_t white_pawns = board.pieces(Color::White, PieceType::Pawn);
    const std::uint64_t black_pawns = board.pieces(Color::Black, PieceType::Pawn);
    const std::uint64_t mixed = (white_pawns * 0x9E3779B185EBCA87ULL) ^ (black_pawns * 0xC2B2AE3D27D4EB4FULL);
    return make_correction_history_key(mover, static_cast<std::size_t>(mixed));
}

int apply_correction_history_to_static_eval(
    int raw_static_eval, const SearchHistory::CorrectionHistory &correction_history,
    const std::optional<CorrectionHistoryKey> &key) {
    if (!key.has_value()) {
        return raw_static_eval;
    }
    const int correction = correction_history.score(*key);
    if (correction == 0) {
        return raw_static_eval;
    }
    return raw_static_eval + correction;
}

int apply_correction_history_to_static_eval(
    int raw_static_eval, const SearchHistory &history, const std::optional<CorrectionHistoryKey> &key) {
    return apply_correction_history_to_static_eval(raw_static_eval, history.correction_history(), key);
}

CaptureNoisyHistoryUpdate make_capture_noisy_history_update(
    const std::optional<CaptureHistoryKey> &capture_key, const std::optional<NoisyHistoryKey> &noisy_key,
    bool success, int depth) {
    CaptureNoisyHistoryUpdate update{};
    update.success = success;
    update.bonus = history_bonus_for_depth(depth);

    if (capture_key.has_value()) {
        const bool valid_square = capture_key->to >= 0 && capture_key->to < 64;
        if (valid_square && is_valid_color(capture_key->mover)) {
            update.target = CaptureNoisyHistoryUpdateTarget::Capture;
            update.capture_key = capture_key;
            return update;
        }
    }

    if (noisy_key.has_value()) {
        const bool valid_square = noisy_key->to >= 0 && noisy_key->to < 64;
        if (valid_square && is_valid_color(noisy_key->mover)) {
            update.target = CaptureNoisyHistoryUpdateTarget::Noisy;
            update.noisy_key = noisy_key;
        }
    }

    return update;
}

CaptureNoisyHistoryUpdateEvent make_capture_noisy_history_update_event_for_tests(
    CaptureNoisyHistoryUpdateTarget target, const std::optional<CaptureHistoryKey> &capture_key,
    const std::optional<NoisyHistoryKey> &noisy_key, int depth, bool success, const char *reason) {
    CaptureNoisyHistoryUpdateEvent event{};
    event.target = target;
    event.capture_key = capture_key;
    event.noisy_key = noisy_key;
    event.depth = depth;
    event.success = success;
    event.reason = reason != nullptr ? reason : "";
    return event;
}

void apply_capture_noisy_history_update_event_for_tests(SearchHistory &history,
                                                        const CaptureNoisyHistoryUpdateEvent &event) {
    if (event.target == CaptureNoisyHistoryUpdateTarget::Capture) {
        const auto update = make_capture_noisy_history_update(event.capture_key, std::nullopt, event.success, event.depth);
        apply_capture_noisy_history_update_for_tests(history, update);
        return;
    }

    if (event.target == CaptureNoisyHistoryUpdateTarget::Noisy) {
        const auto update = make_capture_noisy_history_update(std::nullopt, event.noisy_key, event.success, event.depth);
        apply_capture_noisy_history_update_for_tests(history, update);
        return;
    }

    const auto update = make_capture_noisy_history_update(std::nullopt, std::nullopt, event.success, event.depth);
    apply_capture_noisy_history_update_for_tests(history, update);
}

void apply_capture_noisy_history_update(SearchHistory &history, const CaptureNoisyHistoryUpdate &update) {
    switch (update.target) {
        case CaptureNoisyHistoryUpdateTarget::Capture:
            if (update.capture_key.has_value()) {
                history.capture_history().update(update.capture_key->mover, move_from_capture_key(*update.capture_key),
                                                 update.bonus, update.success);
            }
            break;
        case CaptureNoisyHistoryUpdateTarget::Noisy:
            if (update.noisy_key.has_value()) {
                history.noisy_history().update(update.noisy_key->mover, move_from_noisy_key(*update.noisy_key),
                                               update.bonus, update.success);
            }
            break;
        case CaptureNoisyHistoryUpdateTarget::None:
            break;
    }
    if (update.target != CaptureNoisyHistoryUpdateTarget::None) {
        history.record_capture_noisy_runtime_update_applied();
    }
}

void apply_capture_noisy_history_update_for_tests(SearchHistory &history, const CaptureNoisyHistoryUpdate &update) {
    apply_capture_noisy_history_update(history, update);
}

bool apply_capture_noisy_runtime_update_for_tests(SearchHistory &history, CaptureNoisyRuntimeUpdateSite site,
                                                  const std::optional<CaptureHistoryKey> &capture_key,
                                                  const std::optional<NoisyHistoryKey> &noisy_key, int depth) {
    if (site != CaptureNoisyRuntimeUpdateSite::MainNegamaxTacticalBetaCutoff) {
        return false;
    }
    const auto update = make_capture_noisy_history_update(capture_key, noisy_key, true, depth);
    if (update.target == CaptureNoisyHistoryUpdateTarget::None) {
        return false;
    }
    apply_capture_noisy_history_update(history, update);
    return true;
}

bool apply_continuation_runtime_update_for_tests(
    SearchHistory &history, ContinuationRuntimeUpdateSite site,
    const std::optional<ContinuationHistoryKey> &continuation_key,
    const std::span<const ContinuationHistoryKey> &tried_quiet_keys, int depth) {
    if (site != ContinuationRuntimeUpdateSite::MainNegamaxQuietBetaCutoff) {
        return false;
    }
    if (!continuation_key.has_value()) {
        history.record_continuation_quiet_beta_cutoff_skip_for_tests();
        return false;
    }
    Move previous_move{};
    previous_move.piece = continuation_key->previous_moving_piece;
    previous_move.to = continuation_key->previous_to_square;
    Move current_move{};
    current_move.piece = continuation_key->current_moving_piece;
    current_move.to = continuation_key->current_to_square;
    history.continuation_history().update(continuation_key->previous_mover_color, previous_move,
                                          continuation_key->current_mover_color, current_move, depth, true);
    history.record_continuation_quiet_beta_cutoff_update_for_tests();

    for (const auto &tried_key : tried_quiet_keys) {
        Move tried_move{};
        tried_move.piece = tried_key.current_moving_piece;
        tried_move.to = tried_key.current_to_square;
        history.continuation_history().update(tried_key.previous_mover_color, previous_move,
                                              tried_key.current_mover_color, tried_move,
                                              search_params::continuation_history_quiet_beta_cutoff_malus, true);
        history.record_continuation_quiet_beta_cutoff_malus_for_tests();
    }
    return true;
}
bool apply_correction_history_quiet_beta_cutoff_update(
    SearchHistory &history, const std::optional<CorrectionHistoryKey> &correction_key, int raw_static_eval,
    int cutoff_value) {
    if (!correction_key.has_value()) {
        return false;
    }
    const int raw_delta = cutoff_value - raw_static_eval;
    if (raw_delta <= 0) {
        return false;
    }
    const int correction_delta = normalize_correction_history_runtime_delta(raw_delta);
    if (correction_delta <= 0) {
        return false;
    }
    history.correction_history().update(*correction_key, correction_delta);
    history.record_correction_quiet_beta_cutoff_update_for_tests();
    return true;
}

bool apply_correction_history_quiet_beta_cutoff_update_for_tests(
    SearchHistory &history, const std::optional<CorrectionHistoryKey> &correction_key, int raw_static_eval,
    int cutoff_value) {
    return apply_correction_history_quiet_beta_cutoff_update(history, correction_key, raw_static_eval, cutoff_value);
}
bool apply_correction_history_fail_low_update(
    SearchHistory &history, const std::optional<CorrectionHistoryKey> &correction_key, int raw_static_eval,
    int best_value) {
    if (!correction_key.has_value()) {
        return false;
    }
    const int raw_delta = best_value - raw_static_eval;
    if (raw_delta >= 0) {
        return false;
    }
    const int correction_delta = normalize_correction_history_runtime_delta(raw_delta);
    if (correction_delta >= 0) {
        return false;
    }
    history.correction_history().update(*correction_key, correction_delta);
    history.record_correction_fail_low_update_for_tests();
    return true;
}

bool apply_correction_history_fail_low_update_for_tests(
    SearchHistory &history, const std::optional<CorrectionHistoryKey> &correction_key, int raw_static_eval,
    int best_value) {
    return apply_correction_history_fail_low_update(history, correction_key, raw_static_eval, best_value);
}

void SearchHistory::reset_capture_noisy_runtime_update_counters() {
    capture_noisy_runtime_update_counters_ = {};
}

void SearchHistory::record_capture_noisy_runtime_update_applied() {
    ++capture_noisy_runtime_update_counters_.applied;
}
int SearchHistory::continuation_quiet_beta_cutoff_update_count_for_tests() const {
    return continuation_runtime_update_counters_.quiet_beta_cutoff_applied;
}
int SearchHistory::continuation_quiet_beta_cutoff_malus_count_for_tests() const {
    return continuation_runtime_update_counters_.quiet_beta_cutoff_malus_applied;
}
int SearchHistory::continuation_quiet_beta_cutoff_skip_count_for_tests() const {
    return continuation_runtime_update_counters_.quiet_beta_cutoff_skipped;
}
void SearchHistory::reset_continuation_runtime_observability_for_tests() {
    continuation_runtime_update_counters_ = {};
}
void SearchHistory::record_continuation_quiet_beta_cutoff_update_for_tests() {
    ++continuation_runtime_update_counters_.quiet_beta_cutoff_applied;
}
void SearchHistory::record_continuation_quiet_beta_cutoff_malus_for_tests() {
    ++continuation_runtime_update_counters_.quiet_beta_cutoff_malus_applied;
}
void SearchHistory::record_continuation_quiet_beta_cutoff_skip_for_tests() {
    ++continuation_runtime_update_counters_.quiet_beta_cutoff_skipped;
}
int SearchHistory::correction_quiet_beta_cutoff_update_count_for_tests() const {
    return correction_runtime_update_counters_.quiet_beta_cutoff_applied;
}
int SearchHistory::correction_fail_low_update_count_for_tests() const {
    return correction_runtime_update_counters_.fail_low_applied;
}
void SearchHistory::reset_correction_runtime_observability_for_tests() {
    correction_runtime_update_counters_ = {};
}
void SearchHistory::record_correction_quiet_beta_cutoff_update_for_tests() {
    ++correction_runtime_update_counters_.quiet_beta_cutoff_applied;
}
void SearchHistory::record_correction_fail_low_update_for_tests() {
    ++correction_runtime_update_counters_.fail_low_applied;
}
int SearchHistory::reverse_futility_return_count_for_tests() const {
    return reverse_futility_runtime_counters_.return_applied;
}
void SearchHistory::record_reverse_futility_return() {
    ++reverse_futility_runtime_counters_.return_applied;
}
void SearchHistory::reset_reverse_futility_runtime_observability_for_tests() {
    reverse_futility_runtime_counters_ = {};
}
int SearchHistory::move_count_pruning_continue_count_for_tests() const {
    return move_count_pruning_runtime_counters_.continue_applied;
}
void SearchHistory::record_move_count_pruning_continue() {
    ++move_count_pruning_runtime_counters_.continue_applied;
}
void SearchHistory::reset_move_count_pruning_runtime_observability_for_tests() {
    move_count_pruning_runtime_counters_ = {};
}
int SearchHistory::probcut_probe_count_for_tests() const {
    return probcut_runtime_counters_.probe_applied;
}
void SearchHistory::record_probcut_probe() {
    ++probcut_runtime_counters_.probe_applied;
}
int SearchHistory::probcut_empty_candidate_context_count_for_tests() const {
    return probcut_runtime_counters_.empty_candidate_context_applied;
}
void SearchHistory::record_probcut_empty_candidate_context() {
    ++probcut_runtime_counters_.empty_candidate_context_applied;
}
int SearchHistory::probcut_cutoff_decision_count_for_tests() const {
    return probcut_runtime_counters_.cutoff_decision_applied;
}
void SearchHistory::record_probcut_cutoff_decision() {
    ++probcut_runtime_counters_.cutoff_decision_applied;
}
void SearchHistory::reset_probcut_runtime_observability_for_tests() {
    probcut_runtime_counters_ = {};
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
    const int bonus = history_bonus_for_depth(depth);
    auto &entry = quiet_history_[color_to_index(mover)][move.from][move.to];
    apply_history_delta(entry, bonus, success);
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

int SearchHistory::CaptureHistory::score(const Move &move, Color mover) const {
    if (!move.captured.has_value()) {
        return 0;
    }
    return table_[color_to_index(mover)][piece_to_index(move.piece)][piece_to_index(*move.captured)][move.to];
}

void SearchHistory::CaptureHistory::update(Color mover, const Move &move, int depth, bool success) {
    if (!move.captured.has_value()) {
        return;
    }
    auto &entry = table_[color_to_index(mover)][piece_to_index(move.piece)][piece_to_index(*move.captured)][move.to];
    apply_history_delta(entry, history_bonus_for_depth(depth), success);
}

void SearchHistory::CaptureHistory::clear() {
    table_ = {};
}

int SearchHistory::NoisyHistory::score(const Move &move, Color mover) const {
    if (is_quiet_move(move)) {
        return 0;
    }
    return table_[color_to_index(mover)][piece_to_index(move.piece)][move.to];
}

void SearchHistory::NoisyHistory::update(Color mover, const Move &move, int depth, bool success) {
    if (is_quiet_move(move)) {
        return;
    }
    auto &entry = table_[color_to_index(mover)][piece_to_index(move.piece)][move.to];
    apply_history_delta(entry, history_bonus_for_depth(depth), success);
}

void SearchHistory::NoisyHistory::clear() {
    table_ = {};
}

int SearchHistory::ContinuationHistory::score(Color previous_mover, const Move &previous_move, Color current_mover,
                                              const Move &current_move) const {
    return table_[color_to_index(previous_mover)][color_to_index(current_mover)][piece_to_index(previous_move.piece)]
                 [previous_move.to][piece_to_index(current_move.piece)][current_move.to];
}

void SearchHistory::ContinuationHistory::update(Color previous_mover, const Move &previous_move, Color current_mover,
                                                const Move &current_move, int depth, bool success) {
    auto &entry = table_[color_to_index(previous_mover)][color_to_index(current_mover)]
                        [piece_to_index(previous_move.piece)][previous_move.to][piece_to_index(current_move.piece)]
                        [current_move.to];
    apply_history_delta(entry, history_bonus_for_depth(depth), success);
}

void SearchHistory::ContinuationHistory::clear() {
    table_ = {};
}

void SearchHistory::CorrectionHistory::clear() {
    table_ = {};
}

bool SearchHistory::CorrectionHistory::is_valid_key(const CorrectionHistoryKey &key) {
    return is_valid_color(key.mover_color);
}

std::size_t SearchHistory::CorrectionHistory::normalize_bucket(std::size_t bucket) {
    return bucket % bucket_count_;
}

int SearchHistory::CorrectionHistory::score(const CorrectionHistoryKey &key) const {
    if (!is_valid_key(key)) {
        return 0;
    }
    return table_[color_to_index(key.mover_color)][normalize_bucket(key.bucket)];
}
int SearchHistory::CorrectionHistory::score(Color mover, std::size_t bucket) const {
    return score(CorrectionHistoryKey{mover, bucket});
}

void SearchHistory::CorrectionHistory::update(const CorrectionHistoryKey &key, int bonus) {
    if (!is_valid_key(key)) {
        return;
    }
    auto &entry = table_[color_to_index(key.mover_color)][normalize_bucket(key.bucket)];
    entry = std::clamp(entry + bonus, search_params::correction_history_min, search_params::correction_history_max);
}
void SearchHistory::CorrectionHistory::update(Color mover, std::size_t bucket, int depth, bool success) {
    const int raw_bonus = history_bonus_for_depth(depth);
    update(CorrectionHistoryKey{mover, bucket}, success ? raw_bonus : -raw_bonus);
}

void SearchHistory::clear() {
    killer_moves_ = {};
    quiet_history_ = {};
    capture_history_.clear();
    noisy_history_.clear();
    continuation_history_.clear();
    correction_history_.clear();
    reset_capture_noisy_runtime_update_counters();
    reset_continuation_runtime_observability_for_tests();
    reset_correction_runtime_observability_for_tests();
    reset_reverse_futility_runtime_observability_for_tests();
    reset_move_count_pruning_runtime_observability_for_tests();
    reset_probcut_runtime_observability_for_tests();
}

}  // namespace sirio
