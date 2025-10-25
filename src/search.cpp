#include "sirio/search.hpp"

#include <limits>

#include "sirio/endgame.hpp"
#include "sirio/evaluation.hpp"
#include "sirio/movegen.hpp"

namespace sirio {

namespace {

constexpr int mate_score = 100000;

int negamax(const Board &board, int depth, int alpha, int beta, int ply, Move *best_move,
            bool *found_best) {
    if (!sufficient_material_to_force_checkmate(board)) {
        return 0;
    }

    if (depth == 0) {
        int eval = evaluate(board);
        return board.side_to_move() == Color::White ? eval : -eval;
    }

    auto moves = generate_legal_moves(board);
    if (moves.empty()) {
        if (board.in_check(board.side_to_move())) {
            return -mate_score + ply;
        }
        return 0;
    }

    int best_score = std::numeric_limits<int>::min();
    Move local_best;
    bool local_found = false;

    for (const Move &move : moves) {
        Board next = board.apply_move(move);
        int score = -negamax(next, depth - 1, -beta, -alpha, ply + 1, nullptr, nullptr);
        if (score > best_score) {
            best_score = score;
            local_best = move;
            local_found = true;
        }
        if (score > alpha) {
            alpha = score;
        }
        if (alpha >= beta) {
            break;
        }
    }

    if (best_move && local_found) {
        *best_move = local_best;
    }
    if (found_best) {
        *found_best = local_found;
    }

    return best_score;
}

}  // namespace

SearchResult search_best_move(const Board &board, int depth) {
    SearchResult result;
    if (depth <= 0) {
        depth = 1;
    }

    Move best_move;
    bool found = false;
    int score = negamax(board, depth, std::numeric_limits<int>::min() / 2,
                        std::numeric_limits<int>::max() / 2, 0, &best_move, &found);
    result.score = score;
    result.has_move = found;
    if (found) {
        result.best_move = best_move;
    }
    return result;
}

}  // namespace sirio

