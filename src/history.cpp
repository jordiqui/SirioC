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
