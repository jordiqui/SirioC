#include "sirio/search.hpp"

#include <algorithm>
#include <array>
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
};

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
    if (!sufficient_material_to_force_checkmate(board)) {
        return 0;
    }

    if (draw_by_fifty_move_rule(board) || draw_by_threefold_repetition(board) ||
        draw_by_insufficient_material_rule(board)) {
        return 0;
    }

    if (depth == 0) {
        return quiescence(board, alpha, beta, ply, context);
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
        if (entry.depth >= depth) {
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

    for (const Move &move : moves) {
        Board next = board.apply_move(move);
        int score =
            -negamax(next, depth - 1, -beta, -alpha, ply + 1, nullptr, nullptr, context);
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

    if (local_found) {
        TTEntry entry;
        entry.best_move = local_best;
        entry.depth = depth;
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

SearchResult search_best_move(const Board &board, int depth) {
    SearchResult result;
    if (depth <= 0) {
        depth = 1;
    }

    Move best_move;
    bool found = false;
    SearchContext context;
    int score = negamax(board, depth, std::numeric_limits<int>::min() / 2,
                        std::numeric_limits<int>::max() / 2, 0, &best_move, &found, context);
    result.score = score;
    result.has_move = found;
    if (found) {
        result.best_move = best_move;
    }
    return result;
}

}  // namespace sirio

