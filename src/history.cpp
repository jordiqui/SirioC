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
    return CorrectionHistoryKey{mover_color, bucket % 1024};
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

void apply_capture_noisy_history_update_for_tests(SearchHistory &history, const CaptureNoisyHistoryUpdate &update) {
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

std::size_t SearchHistory::CorrectionHistory::normalize_bucket(std::size_t bucket) {
    return bucket % bucket_count_;
}

int SearchHistory::CorrectionHistory::score(Color mover, std::size_t bucket) const {
    return table_[color_to_index(mover)][normalize_bucket(bucket)];
}

void SearchHistory::CorrectionHistory::update(Color mover, std::size_t bucket, int depth, bool success) {
    auto &entry = table_[color_to_index(mover)][normalize_bucket(bucket)];
    apply_history_delta(entry, history_bonus_for_depth(depth), success);
}

void SearchHistory::clear() {
    killer_moves_ = {};
    quiet_history_ = {};
    capture_history_.clear();
    noisy_history_.clear();
    continuation_history_.clear();
    correction_history_.clear();
}

}  // namespace sirio
