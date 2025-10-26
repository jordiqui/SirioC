#include "sirio/search.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "sirio/draws.hpp"
#include "sirio/endgame.hpp"
#include "sirio/evaluation.hpp"
#include "sirio/movegen.hpp"

namespace sirio {

namespace {

constexpr int mate_score = 100000;
constexpr int max_search_depth = 64;
constexpr int mate_threshold = mate_score - max_search_depth;

constexpr std::array<int, static_cast<std::size_t>(PieceType::Count)> mvv_values = {
    100, 320, 330, 500, 900, 20000};

enum class TTNodeType { Exact, LowerBound, UpperBound };

struct TTEntry {
    Move best_move{};
    int depth = 0;
    int score = 0;
    TTNodeType type = TTNodeType::Exact;
};

struct SearchContext {
    std::array<std::array<std::optional<Move>, 2>, max_search_depth> killer_moves{};
    std::unordered_map<std::uint64_t, TTEntry> tt_entries{};
    bool stop = false;
    bool has_time_limit = false;
    bool soft_limit_reached = false;
    std::chrono::steady_clock::time_point start_time{};
    std::chrono::milliseconds soft_time_limit{0};
    std::chrono::milliseconds hard_time_limit{0};
    std::uint64_t node_counter = 0;
};

constexpr std::uint64_t time_check_interval = 2048;

bool same_move(const Move &lhs, const Move &rhs) {
    return lhs.from == rhs.from && lhs.to == rhs.to && lhs.piece == rhs.piece &&
           lhs.captured == rhs.captured && lhs.promotion == rhs.promotion &&
           lhs.is_en_passant == rhs.is_en_passant &&
           lhs.is_castling == rhs.is_castling;
}

bool is_quiet(const Move &move) {
    return !move.captured.has_value() && !move.promotion.has_value() && !move.is_castling &&
           !move.is_en_passant;
}

int mvv_lva_score(const Move &move) {
    if (!move.captured.has_value()) {
        return 0;
    }
    auto victim_index = static_cast<std::size_t>(*move.captured);
    auto attacker_index = static_cast<std::size_t>(move.piece);
    return mvv_values[victim_index] * 100 - mvv_values[attacker_index];
}

int killer_score(const Move &move, const SearchContext &context, int ply) {
    if (ply >= max_search_depth) {
        return 0;
    }
    const auto &slots = context.killer_moves[static_cast<std::size_t>(ply)];
    for (std::size_t index = 0; index < slots.size(); ++index) {
        if (slots[index].has_value() && same_move(*slots[index], move)) {
            return 800000 - static_cast<int>(index);
        }
    }
    return 0;
}

int score_move(const Move &move, const SearchContext &context, int ply,
               const std::optional<Move> &tt_move) {
    if (tt_move.has_value() && same_move(*tt_move, move)) {
        return 1'000'000;
    }
    if (move.captured.has_value()) {
        return 900000 + mvv_lva_score(move);
    }
    return killer_score(move, context, ply);
}

bool should_stop(SearchContext &context) {
    if (context.stop) {
        return true;
    }
    if (!context.has_time_limit) {
        return false;
    }
    ++context.node_counter;
    if ((context.node_counter & (time_check_interval - 1)) != 0) {
        return false;
    }
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - context.start_time);
    if (!context.soft_limit_reached && elapsed >= context.soft_time_limit) {
        context.soft_limit_reached = true;
    }
    if (elapsed >= context.hard_time_limit) {
        context.stop = true;
        return true;
    }
    return false;
}

int evaluate_for_current_player(const Board &board) {
    int eval = evaluate(board);
    return board.side_to_move() == Color::White ? eval : -eval;
}

int to_tt_score(int score, int ply) {
    if (score > mate_threshold) {
        return score + ply;
    }
    if (score < -mate_threshold) {
        return score - ply;
    }
    return score;
}

int from_tt_score(int score, int ply) {
    if (score > mate_threshold) {
        return score - ply;
    }
    if (score < -mate_threshold) {
        return score + ply;
    }
    return score;
}

void order_moves(std::vector<Move> &moves, const SearchContext &context, int ply,
                 const std::optional<Move> &tt_move) {
    std::vector<std::pair<int, Move>> scored;
    scored.reserve(moves.size());
    for (const Move &move : moves) {
        scored.emplace_back(score_move(move, context, ply, tt_move), move);
    }
    std::stable_sort(scored.begin(), scored.end(),
                     [](const auto &lhs, const auto &rhs) { return lhs.first > rhs.first; });
    moves.clear();
    moves.reserve(scored.size());
    for (auto &entry : scored) {
        moves.push_back(entry.second);
    }
}

void store_killer(const Move &move, SearchContext &context, int ply) {
    if (!is_quiet(move) || ply >= max_search_depth) {
        return;
    }
    auto &slots = context.killer_moves[static_cast<std::size_t>(ply)];
    if (!slots[0].has_value() || !same_move(*slots[0], move)) {
        slots[1] = slots[0];
        slots[0] = move;
    }
}

int quiescence(const Board &board, int alpha, int beta, int ply, SearchContext &context);

int negamax(const Board &board, int depth, int alpha, int beta, int ply, Move *best_move,
            bool *found_best, SearchContext &context) {
    if (should_stop(context)) {
        return evaluate_for_current_player(board);
    }
    if (!sufficient_material_to_force_checkmate(board)) {
        return 0;
    }

    if (draw_by_fifty_move_rule(board) || draw_by_threefold_repetition(board) ||
        draw_by_insufficient_material_rule(board)) {
        return 0;
    }

    bool in_check = board.in_check(board.side_to_move());
    int static_eval = 0;
    if (!in_check) {
        static_eval = evaluate_for_current_player(board);
    }
    int max_remaining_depth = max_search_depth - ply;
    int depth_left = std::min(depth, max_remaining_depth);
    if (in_check && depth_left < max_remaining_depth) {
        ++depth_left;
    }

    if (depth_left <= 0) {
        return quiescence(board, alpha, beta, ply, context);
    }

    if (!in_check && depth_left == 1) {
        int futility_margin = 150;
        if (static_eval - futility_margin >= beta) {
            return static_eval - futility_margin;
        }
        if (static_eval + futility_margin <= alpha) {
            return static_eval + futility_margin;
        }
    }

    auto moves = generate_legal_moves(board);
    if (moves.empty()) {
        if (board.in_check(board.side_to_move())) {
            return -mate_score + ply;
        }
        return 0;
    }

    const std::uint64_t hash = board.zobrist_hash();
    std::optional<Move> tt_move;
    auto tt_it = context.tt_entries.find(hash);
    if (tt_it != context.tt_entries.end()) {
        tt_move = tt_it->second.best_move;
        const TTEntry &entry = tt_it->second;
        if (entry.depth >= depth_left) {
            int tt_score = from_tt_score(entry.score, ply);
            switch (entry.type) {
                case TTNodeType::Exact:
                    if (best_move) {
                        *best_move = entry.best_move;
                    }
                    if (found_best) {
                        *found_best = true;
                    }
                    return tt_score;
                case TTNodeType::LowerBound:
                    alpha = std::max(alpha, tt_score);
                    break;
                case TTNodeType::UpperBound:
                    beta = std::min(beta, tt_score);
                    break;
            }
            if (alpha >= beta) {
                if (best_move) {
                    *best_move = entry.best_move;
                }
                if (found_best) {
                    *found_best = true;
                }
                return tt_score;
            }
        }
    }

    order_moves(moves, context, ply, tt_move);

    int alpha_original = alpha;
    int beta_original = beta;
    int best_score = std::numeric_limits<int>::min();
    Move local_best;
    bool local_found = false;

    int moves_searched = 0;
    int max_child_remaining_base = std::max(0, max_search_depth - (ply + 1));
    for (const Move &move : moves) {
        ++moves_searched;
        Board next = board.apply_move(move);
        bool gives_check = next.in_check(next.side_to_move());

        int extension = 0;
        int max_child_remaining = max_child_remaining_base;
        if (gives_check && (depth_left - 1) < max_child_remaining) {
            extension = 1;
        }

        int reduction = 0;
        if (!in_check && !gives_check && is_quiet(move) && depth_left >= 3 && moves_searched > 3) {
            reduction = 1;
            if (depth_left >= 5 && moves_searched > 6) {
                reduction = 2;
            }
        }

        int child_depth = depth_left - 1 + extension - reduction;
        if (child_depth > max_child_remaining) {
            child_depth = max_child_remaining;
        }
        if (child_depth < 0) {
            child_depth = 0;
        }

        int score;
        if (child_depth <= 0) {
            score = -quiescence(next, -beta, -alpha, ply + 1, context);
        } else {
            score = -negamax(next, child_depth, -beta, -alpha, ply + 1, nullptr, nullptr, context);
        }
        if (context.stop) {
            break;
        }
        if (score > best_score) {
            best_score = score;
            local_best = move;
            local_found = true;
        }
        if (score > alpha) {
            alpha = score;
        }
        if (alpha >= beta) {
            store_killer(move, context, ply);
            break;
        }
    }

    if (best_move && local_found) {
        *best_move = local_best;
    }
    if (found_best) {
        *found_best = local_found;
    }

    if (context.stop) {
        return best_score;
    }

    if (local_found) {
        TTEntry entry;
        entry.best_move = local_best;
        entry.depth = depth_left;
        entry.score = to_tt_score(best_score, ply);
        if (best_score <= alpha_original) {
            entry.type = TTNodeType::UpperBound;
        } else if (best_score >= beta_original) {
            entry.type = TTNodeType::LowerBound;
        } else {
            entry.type = TTNodeType::Exact;
        }

        auto current = context.tt_entries.find(hash);
        if (current == context.tt_entries.end() || current->second.depth <= entry.depth) {
            context.tt_entries[hash] = entry;
        }
    }

    return best_score;
}

int quiescence(const Board &board, int alpha, int beta, int ply, SearchContext &context) {
    if (should_stop(context)) {
        return alpha;
    }
    int stand_pat = evaluate_for_current_player(board);
    if (stand_pat >= beta) {
        return stand_pat;
    }
    if (stand_pat > alpha) {
        alpha = stand_pat;
    }

    auto moves = generate_legal_moves(board);
    std::vector<Move> tactical_moves;
    tactical_moves.reserve(moves.size());
    for (const Move &move : moves) {
        if (move.captured.has_value() || move.is_en_passant || move.promotion.has_value()) {
            tactical_moves.push_back(move);
        }
    }

    if (tactical_moves.empty()) {
        return alpha;
    }

    order_moves(tactical_moves, context, ply, std::nullopt);

    for (const Move &move : tactical_moves) {
        Board next = board.apply_move(move);
        int score = -quiescence(next, -beta, -alpha, ply + 1, context);
        if (context.stop) {
            return alpha;
        }
        if (score >= beta) {
            return score;
        }
        if (score > alpha) {
            alpha = score;
        }
    }

    return alpha;
}

}  // namespace

struct TimeAllocation {
    std::chrono::milliseconds soft;
    std::chrono::milliseconds hard;
};

std::optional<TimeAllocation> compute_time_allocation(const Board &board,
                                                      const SearchLimits &limits) {
    if (limits.move_time > 0) {
        auto hard = std::chrono::milliseconds{limits.move_time};
        auto soft = std::chrono::milliseconds{std::max<int>(1, limits.move_time * 9 / 10)};
        if (soft > hard) {
            soft = hard;
        }
        return TimeAllocation{soft, hard};
    }

    Color stm = board.side_to_move();
    int time_left = stm == Color::White ? limits.time_left_white : limits.time_left_black;
    int increment = stm == Color::White ? limits.increment_white : limits.increment_black;
    int moves_to_go = limits.moves_to_go > 0 ? limits.moves_to_go : 30;

    if (time_left > 0) {
        int allocation = time_left / std::max(1, moves_to_go);
        if (increment > 0) {
            allocation += increment;
        }
        allocation = std::max(allocation, increment);
        int reserve = std::max(1, time_left / 20);
        int max_allocation = time_left - reserve;
        if (max_allocation <= 0) {
            max_allocation = std::max(1, time_left / 2);
        }
        allocation = std::min(allocation, max_allocation);
        allocation = std::max(allocation, 1);
        auto hard = std::chrono::milliseconds{allocation};
        auto soft = std::chrono::milliseconds{std::max(1, allocation * 9 / 10)};
        if (soft > hard) {
            soft = hard;
        }
        return TimeAllocation{soft, hard};
    }

    if (increment > 0) {
        int allocation = std::max(increment / 2, 1);
        auto hard = std::chrono::milliseconds{allocation};
        return TimeAllocation{hard, hard};
    }

    return std::nullopt;
}

SearchResult search_best_move(const Board &board, const SearchLimits &limits) {
    SearchResult result;
    int max_depth_limit = limits.max_depth > 0 ? limits.max_depth : max_search_depth;
    max_depth_limit = std::min(max_depth_limit, max_search_depth);

    SearchContext context;
    if (auto allocation = compute_time_allocation(board, limits); allocation.has_value()) {
        context.has_time_limit = true;
        context.soft_time_limit = allocation->soft;
        context.hard_time_limit = allocation->hard;
        context.start_time = std::chrono::steady_clock::now();
        if (context.soft_time_limit.count() <= 0) {
            context.soft_time_limit = std::chrono::milliseconds{1};
        }
        if (context.hard_time_limit.count() <= 0) {
            context.hard_time_limit = context.soft_time_limit;
        }
    }

    Move best_move{};
    bool best_found = false;
    for (int depth = 1; depth <= max_depth_limit; ++depth) {
        Move current_best;
        bool found = false;
        int score = negamax(board, depth, std::numeric_limits<int>::min() / 2,
                            std::numeric_limits<int>::max() / 2, 0, &current_best, &found,
                            context);
        if (context.stop) {
            result.timed_out = true;
            break;
        }
        result.score = score;
        if (found) {
            best_move = current_best;
            best_found = true;
            result.best_move = current_best;
            result.has_move = true;
            result.depth_reached = depth;
        }

        if (context.has_time_limit) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - context.start_time);
            if (elapsed >= context.hard_time_limit) {
                context.stop = true;
                result.timed_out = true;
                break;
            }
            if (elapsed >= context.soft_time_limit) {
                break;
            }
        }
    }

    if (best_found) {
        result.best_move = best_move;
    }

    return result;
}

}  // namespace sirio

