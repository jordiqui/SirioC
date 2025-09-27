#include "engine/search/search.hpp"

#include "engine/core/board.hpp"
#include "engine/eval/eval.hpp"
#include "engine/eval/nnue/evaluator.hpp"
#include "engine/util/time.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <thread>

namespace engine {
namespace {

constexpr int kInfiniteScore = 32000;
constexpr int kMateValue = 30000;
constexpr int kMateThreshold = 29000;
constexpr int kMaxPly = 128;

enum BoundFlag : int { TT_EXACT = 0, TT_LOWER = 1, TT_UPPER = 2 };

int piece_value(char piece) {
    switch (std::tolower(static_cast<unsigned char>(piece))) {
    case 'p': return 100;
    case 'n': return 320;
    case 'b': return 330;
    case 'r': return 500;
    case 'q': return 900;
    default: return 0;
    }
}

constexpr int kHistoryMax = 1'000'000;
constexpr int kHistoryMin = -kHistoryMax;

constexpr int history_index(Move move) {
    return move_from(move) * 64 + move_to(move);
}

constexpr std::array<int, 4> kFutilityMargins{0, 120, 200, 320};

} // namespace

struct Search::ThreadData {
    std::vector<std::array<Move, 2>> killers;
    std::array<int, 64 * 64> history{};
};

Search::Search() : nodes_(0), stop_(false) {
    set_hash(16);
    target_time_ms_ = -1;
    nodes_limit_ = -1;
}

void Search::set_threads(int threads) { threads_ = std::max(1, threads); }

void Search::set_hash(int megabytes) {
    size_t bytes = static_cast<size_t>(std::max(1, megabytes)) * 1024ULL * 1024ULL;
    size_t entry_size = sizeof(TTEntry);
    size_t count = 1;
    while ((count * entry_size) < bytes && count < (1ULL << 26)) {
        count <<= 1;
    }
    tt_.assign(count, {});
    tt_mask_ = count - 1;
}

void Search::stop() { stop_.store(true, std::memory_order_relaxed); }

void Search::set_use_syzygy(bool enable) { use_syzygy_ = enable; }

void Search::set_syzygy_path(std::string path) { syzygy_path_ = std::move(path); }

void Search::set_numa_offset(int offset) { numa_offset_ = offset; }

void Search::set_ponder(bool enable) { ponder_ = enable; }

void Search::set_multi_pv(int multi_pv) { multi_pv_ = std::max(1, multi_pv); }

void Search::set_move_overhead(int overhead_ms) { move_overhead_ms_ = std::max(0, overhead_ms); }

void Search::set_eval_file(std::string path) { eval_file_ = std::move(path); }

void Search::set_eval_file_small(std::string path) { eval_file_small_ = std::move(path); }

void Search::set_nnue_evaluator(const nnue::Evaluator* evaluator) { nnue_eval_ = evaluator; }

void Search::set_use_nnue(bool enable) { use_nnue_eval_ = enable; }

Search::Result Search::find_bestmove(Board& board, const Limits& lim) {
    stop_.store(false, std::memory_order_relaxed);
    return search_position(board, lim);
}

Search::Result Search::search_position(Board& board, const Limits& lim) {
    Result result;
    auto start = std::chrono::steady_clock::now();
    nodes_.store(0, std::memory_order_relaxed);
    search_start_ = start;

    auto alloc = time::compute_allocation(lim, board.white_to_move(), move_overhead_ms_);
    target_time_ms_ = alloc.optimal_ms;
    if (alloc.maximum_ms > 0) {
        deadline_ = start + std::chrono::milliseconds(alloc.maximum_ms);
    } else {
        deadline_.reset();
    }
    nodes_limit_ = lim.nodes >= 0 ? lim.nodes : -1;

    auto legal = board.generate_legal_moves();
    if (legal.empty()) {
        result.bestmove = MOVE_NONE;
        result.depth = 0;
        result.nodes = 0;
        result.time_ms = 0;
        deadline_.reset();
        target_time_ms_ = -1;
        nodes_limit_ = -1;
        return result;
    }

    std::vector<Move> root_moves = legal;
    Move best_move = root_moves.front();
    int best_score = 0;

    int depth_limit = lim.depth > 0 ? lim.depth : 64;
    depth_limit = std::min(depth_limit, 64);

    for (int depth = 1; depth <= depth_limit; ++depth) {
        if (stop_.load(std::memory_order_relaxed)) break;

        std::vector<std::pair<Move, int>> scores(root_moves.size());
        std::atomic<size_t> next_index{0};
        size_t thread_count = std::max(1, threads_);
        std::vector<ThreadData> thread_data(thread_count);
        for (auto& td : thread_data) {
            td.killers.assign(kMaxPly, {MOVE_NONE, MOVE_NONE});
            td.history.fill(0);
        }

        auto worker = [&](ThreadData& td) {
            while (!stop_.load(std::memory_order_relaxed)) {
                size_t idx = next_index.fetch_add(1, std::memory_order_relaxed);
                if (idx >= root_moves.size()) break;
                Move move = root_moves[idx];
                Board child = board.after_move(move);
                int score = -negamax(child, depth - 1, -kInfiniteScore, kInfiniteScore, true, 1, td);
                scores[idx] = {move, score};
            }
        };

        std::vector<std::thread> workers;
        workers.reserve(thread_count > 0 ? thread_count - 1 : 0);
        for (size_t t = 1; t < thread_count; ++t) {
            workers.emplace_back(worker, std::ref(thread_data[t]));
        }
        worker(thread_data[0]);
        for (auto& th : workers) th.join();

        if (stop_.load(std::memory_order_relaxed)) break;

        // Determine best move at this depth
        std::sort(scores.begin(), scores.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.second > rhs.second;
        });

        if (!scores.empty()) {
            best_move = scores.front().first;
            best_score = scores.front().second;
            result.bestmove = best_move;
            result.depth = depth;
            result.score = best_score;
            result.is_mate = std::abs(best_score) >= kMateThreshold;

            root_moves.clear();
            for (const auto& [move, score] : scores) {
                root_moves.push_back(move);
            }
        }

        auto now = std::chrono::steady_clock::now();
        if (deadline_ && now >= *deadline_) {
            stop_.store(true, std::memory_order_relaxed);
            break;
        }
        if (target_time_ms_ > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - search_start_).count();
            if (elapsed >= target_time_ms_) {
                break;
            }
        }
    }

    result.nodes = nodes_.load(std::memory_order_relaxed);
    result.time_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start)
            .count());
    result.pv = extract_pv(board, result.bestmove);

    deadline_.reset();
    target_time_ms_ = -1;
    nodes_limit_ = -1;
    return result;
}

int Search::negamax(Board& board, int depth, int alpha, int beta, bool pv_node, int ply,
                    ThreadData& thread_data) {
    if (stop_.load(std::memory_order_relaxed)) return 0;
    if (deadline_ && std::chrono::steady_clock::now() >= *deadline_) {
        stop_.store(true, std::memory_order_relaxed);
        return evaluate(board);
    }

    uint64_t visited = nodes_.fetch_add(1, std::memory_order_relaxed) + 1;
    if (nodes_limit_ >= 0 && visited >= static_cast<uint64_t>(nodes_limit_)) {
        stop_.store(true, std::memory_order_relaxed);
        return evaluate(board);
    }

    bool in_check = board.side_to_move_in_check();
    if (in_check) ++depth;

    if (ply >= static_cast<int>(thread_data.killers.size())) {
        thread_data.killers.resize(ply + 1, {MOVE_NONE, MOVE_NONE});
    }

    if (depth <= 0) {
        return quiescence(board, alpha, beta, ply, thread_data);
    }

    if (use_syzygy_) {
        if (auto tb = probe_syzygy(board)) return *tb;
    }

    int static_eval = evaluate(board);

    Move tt_move = MOVE_NONE;
    int tt_score = 0;
    if (probe_tt(board, depth, alpha, beta, tt_move, tt_score, ply)) {
        return tt_score;
    }

    auto moves = board.generate_legal_moves();
    if (moves.empty()) {
        if (board.side_to_move_in_check()) return -kMateValue + ply;
        return 0;
    }

    auto ordered = order_moves(board, moves, tt_move, ply, thread_data);

    int best_score = -kInfiniteScore;
    Move best_move = MOVE_NONE;
    int alpha_orig = alpha;
    bool first = true;
    int move_count = 0;

    for (Move move : ordered) {
        ++move_count;
        int prev_alpha = alpha;
        bool is_capture = move_is_capture(move);
        bool is_promo = move_promo(move) != 0;
        bool quiet = !is_capture && !is_promo;
        Board::State state;
        board.apply_move(move, state);
        bool gives_check = board.side_to_move_in_check();

        if (!in_check && quiet && depth <= 3 && !gives_check) {
            int margin = kFutilityMargins[static_cast<size_t>(depth)];
            if (static_eval + margin <= alpha) {
                board.undo_move(state);
                continue;
            }
        }

        int score;
        if (first) {
            score = -negamax(board, depth - 1, -beta, -alpha, pv_node, ply + 1, thread_data);
            first = false;
        } else {
            int new_depth = depth - 1;
            bool apply_lmr = new_depth > 0 && depth >= 3 && move_count > 3 && quiet && !gives_check && !pv_node;
            if (apply_lmr) {
                int reduction = 1 + (move_count > 6) + (depth > 5);
                new_depth = std::max(1, new_depth - reduction);
                score = -negamax(board, new_depth, -alpha - 1, -alpha, false, ply + 1, thread_data);
                if (score > alpha) {
                    score = -negamax(board, depth - 1, -alpha - 1, -alpha, false, ply + 1, thread_data);
                }
            } else {
                score = -negamax(board, depth - 1, -alpha - 1, -alpha, false, ply + 1, thread_data);
            }
            if (score > alpha && score < beta) {
                score = -negamax(board, depth - 1, -beta, -alpha, true, ply + 1, thread_data);
            }
        }

        board.undo_move(state);

        if (stop_.load(std::memory_order_relaxed)) return score;

        if (score > best_score) {
            best_score = score;
            best_move = move;
        }
        if (score > alpha) {
            alpha = score;
        }
        bool improved = alpha > prev_alpha;
        if (alpha >= beta) {
            if (quiet) {
                update_killers(thread_data, ply, move);
                update_history(thread_data, move, depth * depth);
            }
            break;
        }
        if (quiet && !improved) {
            update_history(thread_data, move, -std::max(1, depth));
        }
    }

    int flag = TT_EXACT;
    if (best_score <= alpha_orig) flag = TT_UPPER;
    else if (best_score >= beta) flag = TT_LOWER;

    store_tt(board.zobrist_key(), best_move, depth, best_score, flag, ply, static_eval);
    return best_score;
}

int Search::quiescence(Board& board, int alpha, int beta, int ply, ThreadData& thread_data) {
    if (stop_.load(std::memory_order_relaxed)) return alpha;
    if (deadline_ && std::chrono::steady_clock::now() >= *deadline_) {
        stop_.store(true, std::memory_order_relaxed);
        return evaluate(board);
    }

    uint64_t visited = nodes_.fetch_add(1, std::memory_order_relaxed) + 1;
    if (nodes_limit_ >= 0 && visited >= static_cast<uint64_t>(nodes_limit_)) {
        stop_.store(true, std::memory_order_relaxed);
        return evaluate(board);
    }
    int stand_pat = evaluate(board);
    if (stand_pat >= beta) return stand_pat;
    if (stand_pat > alpha) alpha = stand_pat;

    auto moves = board.generate_legal_moves();
    for (Move move : moves) {
        if (!move_is_capture(move) && move_promo(move) == 0) continue;
        Board::State state;
        board.apply_move(move, state);
        int score = -quiescence(board, -beta, -alpha, ply + 1, thread_data);
        board.undo_move(state);
        if (score >= beta) return score;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

bool Search::probe_tt(const Board& board, int depth, int alpha, int beta, Move& tt_move,
                      int& score, int ply) const {
    if (tt_.empty()) return false;
    uint64_t key = board.zobrist_key();
    const TTEntry* entry = nullptr;
    {
        std::shared_lock lock(tt_mutex_);
        TTEntry const& slot = tt_[key & tt_mask_];
        if (slot.key == key) entry = &slot;
    }
    if (!entry) return false;

    tt_move = entry->move;
    score = entry->score;
    if (score > kMateThreshold) score -= ply;
    else if (score < -kMateThreshold) score += ply;

    if (entry->depth >= depth) {
        if (entry->flag == TT_EXACT) return true;
        if (entry->flag == TT_LOWER && score >= beta) return true;
        if (entry->flag == TT_UPPER && score <= alpha) return true;
    }
    return false;
}

void Search::store_tt(uint64_t key, Move best, int depth, int score, int flag, int ply, int eval) {
    if (tt_.empty()) return;
    TTEntry entry;
    entry.key = key;
    entry.move = best;
    entry.depth = static_cast<int8_t>(std::min(depth, 127));
    entry.flag = static_cast<uint8_t>(flag);
    entry.eval = static_cast<int16_t>(std::clamp(eval, -kInfiniteScore, kInfiniteScore));
    if (score > kMateThreshold) score += ply;
    else if (score < -kMateThreshold) score -= ply;
    entry.score = static_cast<int16_t>(std::clamp(score, -kInfiniteScore, kInfiniteScore));

    {
        std::unique_lock lock(tt_mutex_);
        TTEntry& slot = tt_[key & tt_mask_];
        if (slot.depth <= entry.depth || slot.key != key) {
            slot = entry;
        }
    }
}

std::vector<Move> Search::order_moves(const Board& board, std::vector<Move>& moves, Move tt_move,
                                      int ply, const ThreadData& thread_data) const {
    std::vector<std::pair<int, Move>> scored;
    scored.reserve(moves.size());
    for (Move move : moves) {
        int score = 0;
        if (move == tt_move) {
            score = 1'000'000;
        } else if (move_is_capture(move)) {
            char captured = board.piece_on(move_to(move));
            if (move_is_enpassant(move)) captured = board.white_to_move() ? 'p' : 'P';
            char mover = board.piece_on(move_from(move));
            score = 500'000 + piece_value(captured) * 10 - piece_value(mover);
        } else if (move_promo(move) != 0) {
            score = 400'000 + move_promo(move) * 100;
        } else {
            if (ply < static_cast<int>(thread_data.killers.size())) {
                const auto& killers = thread_data.killers[ply];
                if (move == killers[0]) score = 300'000;
                else if (move == killers[1]) score = 299'000;
            }
            if (score == 0) {
                score = 200'000 + thread_data.history[static_cast<size_t>(history_index(move))];
            }
        }
        scored.emplace_back(score, move);
    }
    std::sort(scored.begin(), scored.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first > rhs.first;
    });
    std::vector<Move> ordered;
    ordered.reserve(scored.size());
    for (auto& [score, move] : scored) ordered.push_back(move);
    return ordered;
}

void Search::update_killers(ThreadData& thread_data, int ply, Move move) {
    if (ply >= static_cast<int>(thread_data.killers.size())) {
        thread_data.killers.resize(ply + 1, {MOVE_NONE, MOVE_NONE});
    }
    auto& killers = thread_data.killers[ply];
    if (killers[0] != move) {
        killers[1] = killers[0];
        killers[0] = move;
    }
}

void Search::update_history(ThreadData& thread_data, Move move, int delta) {
    int idx = history_index(move);
    auto& entry = thread_data.history[static_cast<size_t>(idx)];
    entry = std::clamp(entry + delta, kHistoryMin, kHistoryMax);
}

std::vector<Move> Search::extract_pv(const Board& board, Move best) const {
    std::vector<Move> pv;
    if (best == MOVE_NONE) return pv;
    pv.push_back(best);
    Board current = board.after_move(best);
    for (int depth = 1; depth < kMaxPly; ++depth) {
        uint64_t key = current.zobrist_key();
        TTEntry entry;
        bool found = false;
        {
            std::shared_lock lock(tt_mutex_);
            const TTEntry& slot = tt_[key & tt_mask_];
            if (slot.key == key && slot.move != MOVE_NONE) {
                entry = slot;
                found = true;
            }
        }
        if (!found) break;
        Move next = entry.move;
        if (std::find(pv.begin(), pv.end(), next) != pv.end()) break;
        pv.push_back(next);
        current = current.after_move(next);
    }
    return pv;
}

int Search::evaluate(const Board& board) const {
    if (use_nnue_eval_ && nnue_eval_) {
        return nnue_eval_->eval_cp(board);
    }
    return eval::evaluate(board);
}

std::optional<int> Search::probe_syzygy(const Board& board) const {
    if (!use_syzygy_) return std::nullopt;
    int white_non_king = 0;
    int black_non_king = 0;
    char white_piece = 0;
    char black_piece = 0;
    for (char piece : board.squares()) {
        if (piece == '.' || piece == 'K' || piece == 'k') continue;
        if (std::isupper(static_cast<unsigned char>(piece))) {
            ++white_non_king;
            white_piece = piece;
        } else {
            ++black_non_king;
            black_piece = piece;
        }
        if (white_non_king > 1 || black_non_king > 1) return std::nullopt;
    }
    if (white_non_king == 0 && black_non_king == 0) return 0;
    auto tb_value = [](char piece) {
        switch (std::tolower(static_cast<unsigned char>(piece))) {
        case 'q': return 9500;
        case 'r': return 5000;
        case 'b': return 3300;
        case 'n': return 3200;
        case 'p': return 1200;
        default: return 0;
        }
    };
    if (white_non_king == 1 && black_non_king == 0) {
        int val = tb_value(white_piece) + kMateValue / 4;
        return board.white_to_move() ? val : -val;
    }
    if (white_non_king == 0 && black_non_king == 1) {
        int val = tb_value(black_piece) + kMateValue / 4;
        return board.white_to_move() ? -val : val;
    }
    return std::nullopt;
}

} // namespace engine
