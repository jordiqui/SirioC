#include "sirio/search.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include "sirio/draws.hpp"
#include "sirio/endgame.hpp"
#include "sirio/evaluation.hpp"
#include "sirio/move.hpp"
#include "sirio/movegen.hpp"
#include "sirio/time_manager.hpp"
#include "sirio/transposition_table.hpp"
#include "sirio/syzygy.hpp"

namespace sirio {

namespace {

struct SearchSharedState;

constexpr int mate_score = 100000;
constexpr int max_search_depth = 128;
constexpr int mate_threshold = mate_score - max_search_depth;
constexpr std::array<int, static_cast<std::size_t>(PieceType::Count)> mvv_values = {
    100, 320, 330, 500, 900, 20000};
constexpr int quiescence_quiet_check_margin = 300;

std::atomic<int> search_thread_count{1};

std::mutex active_search_mutex;
SearchSharedState *active_search_state = nullptr;
std::atomic_flag info_output_flag = ATOMIC_FLAG_INIT;
std::atomic<bool> stop_requested_pending{false};



struct SearchSharedState {
    std::atomic<bool> stop{false};
    std::atomic<bool> soft_limit_reached{false};
    std::atomic<bool> timed_out{false};
    std::atomic<std::uint64_t> node_counter{0};
    std::atomic<int> background_tasks{0};
    bool has_time_limit = false;
    bool has_node_limit = false;
    std::chrono::steady_clock::time_point start_time{};
    std::chrono::milliseconds soft_time_limit{0};
    std::chrono::milliseconds hard_time_limit{0};
    std::uint64_t node_limit = 0;
    std::mutex background_mutex;
    std::condition_variable background_cv;

    void start_background_tasks(int count) {
        {
            std::lock_guard<std::mutex> lock(background_mutex);
            background_tasks.store(count, std::memory_order_relaxed);
        }
        if (count == 0) {
            background_cv.notify_all();
        }
    }

    void notify_background_task_complete() {
        if (background_tasks.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            std::lock_guard<std::mutex> lock(background_mutex);
            background_cv.notify_all();
        }
    }

    void wait_for_background_tasks() {
        std::unique_lock<std::mutex> lock(background_mutex);
        background_cv.wait(lock, [&]() {
            return background_tasks.load(std::memory_order_acquire) == 0;
        });
    }
};

struct SearchContext {
    std::array<std::array<std::optional<Move>, 2>, max_search_depth> killer_moves{};
    GlobalTranspositionTable *tt = nullptr;
    std::uint8_t tt_generation = 1;
    SearchSharedState *shared = nullptr;
    std::chrono::milliseconds last_iteration_time{0};
    int selective_depth = 0;
    std::uint64_t local_node_accumulator = 0;
    std::uint64_t total_nodes = 0;
    bool is_primary_thread = false;
};

constexpr std::uint64_t node_flush_interval = 512;
constexpr std::chrono::microseconds info_output_lock_timeout{500};

class TimedAtomicFlagLock {
public:
    TimedAtomicFlagLock(std::atomic_flag &flag, std::chrono::microseconds timeout)
        : flag_(flag), acquired_(false) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (flag_.test_and_set(std::memory_order_acquire)) {
            if (timeout.count() <= 0) {
                return;
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                return;
            }
            std::this_thread::yield();
        }
        acquired_ = true;
    }

    ~TimedAtomicFlagLock() {
        if (acquired_) {
            flag_.clear(std::memory_order_release);
        }
    }

    TimedAtomicFlagLock(const TimedAtomicFlagLock &) = delete;
    TimedAtomicFlagLock &operator=(const TimedAtomicFlagLock &) = delete;

    bool owns_lock() const { return acquired_; }

private:
    std::atomic_flag &flag_;
    bool acquired_;
};

struct NodeThroughputMetrics {
    std::uint64_t nps = 0;
    std::uint64_t knps_before = 0;
    std::uint64_t knps_after = 0;
};

std::uint64_t safe_scaled_rate(std::uint64_t numerator, std::uint64_t denominator,
                               std::uint64_t multiplier) {
    if (denominator == 0) {
        return 0;
    }
    long double scaled = static_cast<long double>(numerator) *
                         static_cast<long double>(multiplier) /
                         static_cast<long double>(denominator);
    if (scaled <= 0.0L) {
        return 0;
    }
    if (scaled >= static_cast<long double>(std::numeric_limits<std::uint64_t>::max())) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return static_cast<std::uint64_t>(scaled);
}

NodeThroughputMetrics compute_node_metrics(const SearchSharedState &, std::uint64_t nodes,
                                           std::chrono::nanoseconds elapsed) {
    NodeThroughputMetrics metrics{};
    auto elapsed_ns = static_cast<std::uint64_t>(elapsed.count());
    metrics.nps = safe_scaled_rate(nodes, elapsed_ns, 1'000'000'000ULL);
    metrics.knps_after = safe_scaled_rate(nodes, elapsed_ns, 1'000'000ULL);
    metrics.knps_before = metrics.knps_after;
    return metrics;
}

void flush_thread_node_counter(SearchContext &context) {
    if (context.shared == nullptr) {
        return;
    }
    if (context.local_node_accumulator == 0) {
        return;
    }
    SearchSharedState &shared = *context.shared;
    shared.node_counter.fetch_add(context.local_node_accumulator, std::memory_order_relaxed);
    context.local_node_accumulator = 0;
}

constexpr std::uint64_t time_check_interval = 2048;

}  // namespace

namespace {

constexpr int kMaxSearchThreads = 1024;

int clamp_thread_count(int threads) {
    return std::clamp(threads, 1, kMaxSearchThreads);
}

int env_thread_override() {
    if (const char *env = std::getenv("SIRIOC_THREADS")) {
        char *end = nullptr;
        long value = std::strtol(env, &end, 10);
        if (end != env && *end == '\0' && value > 0) {
            return clamp_thread_count(static_cast<int>(value));
        }
    }
    return 0;
}

}  // namespace

int recommended_search_threads() {
    if (int override = env_thread_override(); override > 0) {
        return override;
    }
    unsigned int hw = std::thread::hardware_concurrency();
    if (hw == 0) {
        hw = 1;
    }
    return clamp_thread_count(static_cast<int>(hw));
}

namespace {

int total_piece_count(const Board &board) {
    int total = 0;
    for (int color_index = 0; color_index < 2; ++color_index) {
        Color color = color_index == 0 ? Color::White : Color::Black;
        for (int type_index = 0; type_index < static_cast<int>(PieceType::Count); ++type_index) {
            total += std::popcount(board.pieces(color, static_cast<PieceType>(type_index)));
        }
    }
    return total;
}

bool has_non_pawn_material(const Board &board, Color color) {
    return (board.pieces(color, PieceType::Queen) | board.pieces(color, PieceType::Rook) |
            board.pieces(color, PieceType::Bishop) | board.pieces(color, PieceType::Knight)) != 0;
}

bool is_pawn_storm_move(const Move &move, Color mover) {
    if (move.piece != PieceType::Pawn) {
        return false;
    }

    const int from_rank = rank_of(move.from);
    const int to_rank = rank_of(move.to);

    if (mover == Color::White) {
        if (to_rank <= from_rank) {
            return false;
        }
        return (to_rank >= 4) && (move.captured.has_value() || (to_rank - from_rank) >= 1);
    }

    if (to_rank >= from_rank) {
        return false;
    }
    return (to_rank <= 3) && (move.captured.has_value() || (from_rank - to_rank) >= 1);
}

bool is_major_decision(const Move &move, Color mover) {
    if (move.piece == PieceType::Queen) {
        return true;
    }
    if (move.piece == PieceType::Pawn) {
        return is_pawn_storm_move(move, mover);
    }
    return false;
}

int syzygy_wdl_to_score(int wdl, int ply) {
    switch (wdl) {
        case 2:
            return mate_score - ply;
        case 1:
            return 200;
        case 0:
            return 0;
        case -1:
            return -200;
        case -2:
        default:
            return -mate_score + ply;
    }
}

bool same_move(const Move &lhs, const Move &rhs) {
    return lhs.from == rhs.from && lhs.to == rhs.to && lhs.piece == rhs.piece &&
           lhs.captured == rhs.captured && lhs.promotion == rhs.promotion &&
           lhs.is_en_passant == rhs.is_en_passant &&
           lhs.is_castling == rhs.is_castling;
}

std::optional<TTEntry> probe_transposition(GlobalTranspositionTable &tt, std::uint64_t hash,
                                           std::uint8_t expected_generation) {
    for (int attempt = 0; attempt < 2; ++attempt) {
        auto entry_opt = tt.probe(hash);
        if (!entry_opt.has_value()) {
            return std::nullopt;
        }
        if (entry_opt->generation == 0 || entry_opt->generation == expected_generation) {
            return entry_opt;
        }
    }
    return std::nullopt;
}

std::optional<TTEntry> probe_transposition(GlobalTranspositionTable *tt, std::uint64_t hash,
                                           std::uint8_t expected_generation) {
    if (tt == nullptr) {
        return std::nullopt;
    }
    return probe_transposition(*tt, hash, expected_generation);
}

std::vector<Move> extract_principal_variation(const Board &board, GlobalTranspositionTable &tt,
                                              std::uint8_t generation, int max_length) {
    std::vector<Move> pv;
    if (max_length <= 0) {
        return pv;
    }
    Board current = board;
    std::unordered_set<std::uint64_t> visited;
    visited.insert(current.zobrist_hash());
    const int limit = std::min(max_length, max_search_depth);
    for (int depth = 0; depth < limit; ++depth) {
        auto entry_opt = probe_transposition(tt, current.zobrist_hash(), generation);
        if (!entry_opt.has_value()) {
            break;
        }
        const TTEntry &entry = *entry_opt;
        auto moves = generate_legal_moves(current);
        auto it = std::find_if(moves.begin(), moves.end(),
                               [&](const Move &candidate) {
                                   return same_move(candidate, entry.best_move);
                               });
        if (it == moves.end()) {
            break;
        }
        pv.push_back(*it);
        current = current.apply_move(*it);
        std::uint64_t hash = current.zobrist_hash();
        if (visited.count(hash) != 0) {
            break;
        }
        visited.insert(hash);
    }
    return pv;
}

void announce_search_update(const Board &board, const SearchResult &result,
                            const SearchSharedState &shared_state) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - shared_state.start_time);
    auto elapsed_ms_duration = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_ns);
    long long elapsed_ms = elapsed_ms_duration.count();
    if (elapsed_ms < 0) {
        elapsed_ms = 0;
    }
    std::uint64_t nodes = shared_state.node_counter.load(std::memory_order_relaxed);
    NodeThroughputMetrics metrics = compute_node_metrics(shared_state, nodes, elapsed_ns);
    std::uint64_t nps = result.nodes_per_second > 0 ? result.nodes_per_second : metrics.nps;
    std::uint64_t knps_before = result.knps_before > 0 ? result.knps_before : metrics.knps_before;
    std::uint64_t knps_after = result.knps_after > 0 ? result.knps_after : metrics.knps_after;
    int depth = result.depth_reached;
    int seldepth = result.seldepth > 0 ? result.seldepth : depth;
    std::string pv_string = principal_variation_to_uci(board, result.principal_variation);
    TimedAtomicFlagLock lock(info_output_flag, info_output_lock_timeout);
    if (!lock.owns_lock()) {
        return;
    }
    std::cout << "info depth " << depth << " seldepth " << seldepth << " multipv 1 score "
              << format_uci_score(result.score) << " nodes " << nodes << " nps " << nps
              << " knps_before " << knps_before << " knps_after " << knps_after
              << " hashfull 0 tbhits 0 time " << elapsed_ms;
    if (!pv_string.empty()) {
        std::cout << " pv " << pv_string;
    }
    std::cout << std::endl;
}

void announce_currmove(const Move &move, int move_index, const SearchContext &context) {
    if (!context.is_primary_thread || context.shared == nullptr) {
        return;
    }

    const SearchSharedState &shared_state = *context.shared;
    std::uint64_t nodes =
        shared_state.node_counter.load(std::memory_order_relaxed) + context.local_node_accumulator;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - shared_state.start_time);
    long long elapsed_ms = elapsed.count();
    if (elapsed_ms < 0) {
        elapsed_ms = 0;
    }

    TimedAtomicFlagLock lock(info_output_flag, info_output_lock_timeout);
    if (!lock.owns_lock()) {
        return;
    }

    std::cout << "info currmove " << move_to_uci(move) << " currmovenumber " << move_index;
    if (nodes > 0) {
        std::cout << " nodes " << nodes;
    }
    std::cout << " time " << elapsed_ms << std::endl;
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
    SearchSharedState &shared = *context.shared;
    ++context.local_node_accumulator;
    ++context.total_nodes;

    if (context.local_node_accumulator >= node_flush_interval) {
        flush_thread_node_counter(context);
    }

    if (shared.stop.load(std::memory_order_relaxed)) {
        flush_thread_node_counter(context);
        return true;
    }

    std::uint64_t aggregated_nodes =
        shared.node_counter.load(std::memory_order_relaxed) + context.local_node_accumulator;
    if (shared.has_node_limit && aggregated_nodes >= shared.node_limit) {
        flush_thread_node_counter(context);
        shared.stop.store(true, std::memory_order_relaxed);
        return true;
    }

    if (!shared.has_time_limit) {
        return false;
    }

    if ((context.total_nodes & (time_check_interval - 1)) != 0) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - shared.start_time);
    if (!shared.soft_limit_reached.load(std::memory_order_relaxed) &&
        elapsed >= shared.soft_time_limit) {
        shared.soft_limit_reached.store(true, std::memory_order_relaxed);
    }
    if (elapsed >= shared.hard_time_limit) {
        shared.stop.store(true, std::memory_order_relaxed);
        shared.timed_out.store(true, std::memory_order_relaxed);
        flush_thread_node_counter(context);
        return true;
    }
    return shared.stop.load(std::memory_order_relaxed);
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

int quiescence(Board &board, int alpha, int beta, int ply, SearchContext &context);

int negamax(Board &board, int depth, int alpha, int beta, int ply, Move *best_move,
            bool *found_best, SearchContext &context, int parent_static_eval,
            bool allow_null_move) {
    context.selective_depth = std::max(context.selective_depth, ply + 1);
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

    const std::uint64_t hash = board.zobrist_hash();
    bool in_check = board.in_check(board.side_to_move());
    int static_eval = 0;
    if (!in_check) {
        static_eval = evaluate_for_current_player(board);
    }

    const int max_remaining_depth = max_search_depth - ply;
    int depth_left = std::min(depth, max_remaining_depth);
    if (in_check && depth_left < max_remaining_depth) {
        ++depth_left;
    }

    std::optional<TTEntry> tt_entry = probe_transposition(context.tt, hash, context.tt_generation);
    std::optional<Move> tt_move;
    if (tt_entry.has_value()) {
        tt_move = tt_entry->best_move;
        if (!in_check && tt_entry->static_eval != 0) {
            static_eval = tt_entry->static_eval;
        }
    }

    int piece_count = total_piece_count(board);
    if (syzygy::available() && piece_count <= syzygy::probe_piece_limit() &&
        syzygy::max_pieces() >= piece_count && depth_left <= syzygy::probe_depth_limit()) {
        if (auto tb = syzygy::probe_wdl(board); tb.has_value()) {
            int tb_score = syzygy_wdl_to_score(tb->wdl, ply);
            if (std::abs(tb_score) >= mate_threshold || tb->wdl == 0) {
                if (best_move && tb->best_move) {
                    *best_move = *tb->best_move;
                }
                if (found_best && tb->best_move) {
                    *found_best = true;
                }
                return tb_score;
            }
            if (!in_check) {
                static_eval = tb_score;
            }
        }
    }

    if (depth_left <= 0) {
        return quiescence(board, alpha, beta, ply, context);
    }

    if (tt_entry.has_value() && tt_entry->depth >= depth_left) {
        int tt_score = from_tt_score(tt_entry->score, ply);
        switch (tt_entry->type) {
            case TTNodeType::Exact:
                if (best_move) {
                    *best_move = tt_entry->best_move;
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
                *best_move = tt_entry->best_move;
            }
            if (found_best) {
                *found_best = true;
            }
            return tt_score;
        }
    }

    if (!in_check && depth_left == 1) {
        constexpr int futility_margin = 150;
        if (static_eval - futility_margin >= beta) {
            return static_eval - futility_margin;
        }
    }

    if (allow_null_move && !in_check && depth_left >= 3 && static_eval >= beta &&
        has_non_pawn_material(board, board.side_to_move())) {
        Board::NullUndoState null_undo;
        board.make_null_move(null_undo);
        int reduction = 2 + depth_left / 4;
        int null_depth = depth_left - 1 - reduction;
        if (null_depth >= 0) {
            int null_score = -negamax(board, null_depth, -beta, -beta + 1, ply + 1, nullptr, nullptr,
                                      context, static_eval, false);
            board.undo_null_move(null_undo);
            if (context.shared->stop.load(std::memory_order_relaxed)) {
                return 0;
            }
            if (null_score >= beta) {
                return beta;
            }
        } else {
            board.undo_null_move(null_undo);
        }
    }

    auto moves = generate_legal_moves(board);
    if (moves.empty()) {
        if (in_check) {
            return -mate_score + ply;
        }
        return 0;
    }

    order_moves(moves, context, ply, tt_move);

    int alpha_original = alpha;
    int best_score = std::numeric_limits<int>::min();
    Move local_best{};
    bool local_found = false;

    int move_index = 0;
    for (const Move &move : moves) {
        ++move_index;
        if (ply == 0) {
            announce_currmove(move, move_index, context);
        }
        Color mover = board.side_to_move();
        Board::UndoState undo;
        board.make_move(move, undo);
        bool gives_check = board.in_check(board.side_to_move());

        int child_depth = depth_left - 1;
        if (gives_check && child_depth < max_search_depth - (ply + 1)) {
            ++child_depth;
        }
        if (is_pawn_storm_move(move, mover) && child_depth < max_search_depth - (ply + 1)) {
            ++child_depth;
        }
        child_depth = std::min(child_depth, std::max(0, max_search_depth - (ply + 1)));

        int reduction = 0;
        if (child_depth > 0 && depth_left >= 3 && move_index > 1 && is_quiet(move) && !gives_check) {
            bool improving = static_eval > parent_static_eval;
            reduction = 1;
            if (depth_left >= 5 && move_index > 4) {
                ++reduction;
            }
            if (!improving) {
                ++reduction;
            }
            if (reduction > child_depth - 1) {
                reduction = child_depth - 1;
            }
            if (reduction < 0) {
                reduction = 0;
            }
        }

        int new_depth = std::max(0, child_depth - reduction);
        int score;
        if (new_depth <= 0) {
            score = -quiescence(board, -beta, -alpha, ply + 1, context);
        } else {
            score = -negamax(board, new_depth, -beta, -alpha, ply + 1, nullptr, nullptr, context,
                             static_eval, true);
        }
        board.undo_move(move, undo);
        if (context.shared->stop.load(std::memory_order_relaxed)) {
            return 0;
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
            if (is_quiet(move)) {
                store_killer(move, context, ply);
            }
            break;
        }
    }

    if (best_move && local_found) {
        *best_move = local_best;
    }
    if (found_best) {
        *found_best = local_found;
    }

    if (context.shared->stop.load(std::memory_order_relaxed)) {
        return best_score;
    }

    if (local_found) {
        TTEntry new_entry{};
        new_entry.best_move = local_best;
        new_entry.depth = depth_left;
        new_entry.score = to_tt_score(best_score, ply);
        new_entry.static_eval = static_eval;
        if (best_score <= alpha_original) {
            new_entry.type = TTNodeType::UpperBound;
        } else if (best_score >= beta) {
            new_entry.type = TTNodeType::LowerBound;
        } else {
            new_entry.type = TTNodeType::Exact;
        }
        if (context.tt != nullptr) {
            context.tt->store(hash, new_entry, context.tt_generation);
        }
    }

    return best_score;
}

int quiescence(Board &board, int alpha, int beta, int ply, SearchContext &context) {
    context.selective_depth = std::max(context.selective_depth, ply + 1);
    if (should_stop(context)) {
        return alpha;
    }
    if (syzygy::available() && syzygy::max_pieces() >= total_piece_count(board)) {
        if (auto tb = syzygy::probe_wdl(board); tb.has_value()) {
            return syzygy_wdl_to_score(tb->wdl, ply);
        }
    }
    const bool in_check = board.in_check(board.side_to_move());
    int stand_pat = evaluate_for_current_player(board);
    if (!in_check) {
        if (stand_pat >= beta) {
            return stand_pat;
        }
        if (stand_pat > alpha) {
            alpha = stand_pat;
        }
    }

    auto tactical_moves = generate_pseudo_legal_tactical_moves(board);
    {
        auto pseudo_moves = generate_pseudo_legal_moves(board);
        for (const Move &move : pseudo_moves) {
            if (move.captured.has_value() || move.promotion.has_value() || move.is_castling) {
                continue;
            }
            Board::UndoState undo;
            try {
                board.make_move(move, undo);
            } catch (const std::exception &) {
                continue;
            }

            Color mover = opposite(board.side_to_move());
            if (board.king_square(mover) >= 0 && board.in_check(mover)) {
                board.undo_move(move, undo);
                continue;
            }

            if (board.in_check(board.side_to_move())) {
                tactical_moves.push_back(move);
            }

            board.undo_move(move, undo);
        }
    }
    if (tactical_moves.empty()) {
        return alpha;
    }

    order_moves(tactical_moves, context, ply, std::nullopt);

    bool found_legal = false;
    for (const Move &move : tactical_moves) {
        if (!in_check && !move.captured.has_value() && !move.promotion.has_value() &&
            stand_pat + quiescence_quiet_check_margin <= alpha) {
            continue;
        }
        Board::UndoState undo;
        try {
            board.make_move(move, undo);
        } catch (const std::exception &) {
            continue;
        }

        Color mover = opposite(board.side_to_move());
        if (board.king_square(mover) >= 0 && board.in_check(mover)) {
            board.undo_move(move, undo);
            continue;
        }

        found_legal = true;
        int score = -quiescence(board, -beta, -alpha, ply + 1, context);
        board.undo_move(move, undo);
        if (context.shared->stop.load(std::memory_order_relaxed)) {
            return alpha;
        }
        if (score >= beta) {
            return score;
        }
        if (score > alpha) {
            alpha = score;
        }
    }

    if (!found_legal) {
        return alpha;
    }

    return alpha;
}

}  // namespace

class SearchThreadPool {
public:
    static SearchThreadPool &instance() {
        static SearchThreadPool instance;
        return instance;
    }

    SearchThreadPool(const SearchThreadPool &) = delete;
    SearchThreadPool &operator=(const SearchThreadPool &) = delete;

    void ensure_size(int desired) {
        if (desired <= 0) {
            desired = 1;
        }
        std::unique_lock<std::mutex> lock(mutex_);
        int current = static_cast<int>(workers_.size());
        if (desired <= current) {
            return;
        }
        int to_add = desired - current;
        for (int i = 0; i < to_add; ++i) {
            workers_.emplace_back([this]() { worker_loop(); });
        }
    }

    void enqueue(SearchSharedState &shared, std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.push_back(Task{std::move(task), &shared});
        }
        cv_.notify_one();
    }

    void notify_search_start(SearchSharedState &) {
        ensure_size(search_thread_count.load(std::memory_order_relaxed));
    }

    void notify_search_end(SearchSharedState &shared) {
        shared.stop.store(true, std::memory_order_relaxed);
        shared.wait_for_background_tasks();
    }

private:
    struct Task {
        std::function<void()> func;
        SearchSharedState *shared;
    };

    SearchThreadPool() {
        ensure_size(recommended_search_threads());
    }

    ~SearchThreadPool() {
        shutdown();
    }

    void worker_loop() {
        while (true) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [&]() { return shutdown_ || !tasks_.empty(); });
                if (shutdown_ && tasks_.empty()) {
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop_front();
            }

            if (task.func) {
                task.func();
            }
            if (task.shared != nullptr) {
                task.shared->notify_background_task_complete();
            }
        }
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        cv_.notify_all();
        for (auto &worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    std::vector<std::thread> workers_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<Task> tasks_;
    bool shutdown_ = false;
};

void set_search_threads(int threads) {
    int clamped = clamp_thread_count(threads);
    search_thread_count.store(clamped, std::memory_order_relaxed);
    SearchThreadPool::instance().ensure_size(clamped);
}

int get_search_threads() { return search_thread_count.load(std::memory_order_relaxed); }

class ActiveSearchGuard {
public:
    explicit ActiveSearchGuard(SearchSharedState *state) : state_(state) {
        SearchThreadPool::instance().notify_search_start(*state_);
        std::lock_guard<std::mutex> lock(active_search_mutex);
        active_search_state = state_;
        if (stop_requested_pending.load(std::memory_order_relaxed)) {
            state_->stop.store(true, std::memory_order_relaxed);
            stop_requested_pending.store(false, std::memory_order_relaxed);
        }
    }

    ~ActiveSearchGuard() {
        SearchThreadPool::instance().notify_search_end(*state_);
        std::lock_guard<std::mutex> lock(active_search_mutex);
        if (active_search_state == state_) {
            active_search_state = nullptr;
        }
    }

    ActiveSearchGuard(const ActiveSearchGuard &) = delete;
    ActiveSearchGuard &operator=(const ActiveSearchGuard &) = delete;

private:
    SearchSharedState *state_;
};

struct TimeAllocation {
    std::chrono::milliseconds soft;
    std::chrono::milliseconds hard;
};

void adjust_time_allocation(std::chrono::milliseconds &soft, std::chrono::milliseconds &hard) {
    const int overhead = std::clamp(get_move_overhead(), 0, 5000);
    const int min_thinking = std::clamp(get_minimum_thinking_time(), 0, 5000);
    const int slow = std::clamp(get_slow_mover(), 10, 1000);
    const auto overhead_ms = std::chrono::milliseconds{overhead};
    const auto minimum_ms = std::chrono::milliseconds{min_thinking};

    auto adjust_single = [&](std::chrono::milliseconds value) {
        if (value <= overhead_ms) {
            value = std::chrono::milliseconds{0};
        } else {
            value -= overhead_ms;
        }
        long long scaled = value.count() * slow / 100;
        if (scaled < minimum_ms.count()) {
            scaled = minimum_ms.count();
        }
        if (scaled <= 0) {
            scaled = minimum_ms.count() > 0 ? minimum_ms.count() : 1;
        }
        return std::chrono::milliseconds{scaled};
    };

    hard = adjust_single(hard);
    soft = adjust_single(soft);
    if (soft > hard) {
        soft = hard;
    }
}

std::uint64_t nodes_budget_for_time(std::chrono::milliseconds hard) {
    const int nodes_per_ms = get_nodestime();
    if (nodes_per_ms <= 0 || hard.count() <= 0) {
        return 0;
    }
    return static_cast<std::uint64_t>(nodes_per_ms) *
           static_cast<std::uint64_t>(hard.count());
}

std::optional<TimeAllocation> compute_time_allocation(const Board &board,
                                                      const SearchLimits &limits) {
    if (limits.move_time > 0) {
        auto hard = std::chrono::milliseconds{limits.move_time};
        auto soft = std::chrono::milliseconds{std::max<int>(1, limits.move_time * 9 / 10)};
        if (soft > hard) {
            soft = hard;
        }
        adjust_time_allocation(soft, hard);
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
        adjust_time_allocation(soft, hard);
        return TimeAllocation{soft, hard};
    }

    if (increment > 0) {
        int allocation = std::max(increment / 2, 1);
        auto hard = std::chrono::milliseconds{allocation};
        auto soft = hard;
        adjust_time_allocation(soft, hard);
        return TimeAllocation{soft, hard};
    }

    return std::nullopt;
}

struct SharedBestResult {
    std::mutex mutex;
    SearchResult result;
};

bool publish_best_result(const SearchResult &candidate, SharedBestResult &shared,
                         const Board &board, GlobalTranspositionTable &tt,
                         std::uint8_t tt_generation, SearchSharedState &shared_state,
                         bool announce_update) {
    if (!candidate.has_move) {
        return false;
    }

    bool should_update = false;
    {
        std::lock_guard lock(shared.mutex);
        if (!shared.result.has_move ||
            candidate.depth_reached > shared.result.depth_reached ||
            (candidate.depth_reached == shared.result.depth_reached &&
             std::abs(candidate.score) >= std::abs(shared.result.score))) {
            should_update = true;
        }
    }

    if (!should_update) {
        return false;
    }

    SearchResult enriched = candidate;
    enriched.principal_variation = extract_principal_variation(
        board, tt, tt_generation, std::max(candidate.depth_reached, 1));
    if (enriched.principal_variation.empty() && candidate.has_move) {
        enriched.principal_variation.push_back(candidate.best_move);
    }
    if (enriched.seldepth < candidate.seldepth) {
        enriched.seldepth = candidate.seldepth;
    }
    std::uint64_t nodes = shared_state.node_counter.load(std::memory_order_relaxed);
    enriched.nodes = nodes;
    auto now = std::chrono::steady_clock::now();
    auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - shared_state.start_time);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_ns);
    NodeThroughputMetrics metrics = compute_node_metrics(shared_state, nodes, elapsed_ns);
    long long elapsed_ms = elapsed.count();
    if (elapsed_ms < 0) {
        elapsed_ms = 0;
    }
    if (elapsed_ms > std::numeric_limits<int>::max()) {
        elapsed_ms = std::numeric_limits<int>::max();
    }
    enriched.time_ms = static_cast<int>(elapsed_ms);
    enriched.nodes_per_second = metrics.nps;
    enriched.knps_before = metrics.knps_before;
    enriched.knps_after = metrics.knps_after;

    {
        std::lock_guard lock(shared.mutex);
        shared.result = enriched;
    }

    if (announce_update) {
        announce_search_update(board, enriched, shared_state);
    }

    return true;
}

SearchResult run_search_thread(Board board, int max_depth_limit, SearchSharedState &shared,
                               SharedBestResult &shared_result, const SearchResult &seed,
                               int thread_index, bool is_primary, GlobalTranspositionTable &tt,
                               std::uint8_t tt_generation, bool infinite_search) {
    SearchContext context;
    context.shared = &shared;
    context.tt = &tt;
    context.tt_generation = tt_generation;
    context.is_primary_thread = is_primary;
    SearchResult local = seed;
    Move best_move = seed.has_move ? seed.best_move : Move{};
    bool best_found = seed.has_move;
    int previous_score = seed.score;
    bool have_previous = seed.has_move;
    const Color root_color = board.side_to_move();

    initialize_evaluation(board);

    if (thread_index > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds{15 * thread_index});
    }

    int capped_depth_limit = std::max(1, max_depth_limit);
    for (int depth = 1; depth <= capped_depth_limit; ++depth) {
        if (shared.stop.load(std::memory_order_relaxed)) {
            break;
        }

        const int full_min = std::numeric_limits<int>::min() / 2;
        const int full_max = std::numeric_limits<int>::max() / 2;
        int aspiration_window = 25;
        int alpha = have_previous ? previous_score - aspiration_window : full_min;
        int beta = have_previous ? previous_score + aspiration_window : full_max;

        Move current_best{};
        bool found = false;
        int score = 0;
        auto iteration_start = std::chrono::steady_clock::now();
        context.selective_depth = 0;

        while (true) {
            score = negamax(board, depth, alpha, beta, 0, &current_best, &found, context, 0, true);
            if (shared.stop.load(std::memory_order_relaxed)) {
                local.timed_out = shared.timed_out.load(std::memory_order_relaxed);
                break;
            }
            if (have_previous && score <= alpha) {
                aspiration_window *= 2;
                alpha = std::max(full_min, previous_score - aspiration_window);
                beta = std::min(full_max, previous_score + aspiration_window);
                continue;
            }
            if (have_previous && score >= beta) {
                aspiration_window *= 2;
                beta = std::min(full_max, previous_score + aspiration_window);
                alpha = std::max(full_min, previous_score - aspiration_window);
                continue;
            }
            break;
        }

        auto iteration_end = std::chrono::steady_clock::now();
        context.last_iteration_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            iteration_end - iteration_start);

        if (shared.stop.load(std::memory_order_relaxed)) {
            break;
        }

        previous_score = score;
        have_previous = true;
        local.score = score;
        local.seldepth = std::max(local.seldepth, context.selective_depth);
        if (found) {
            best_move = current_best;
            best_found = true;
            local.best_move = current_best;
            local.has_move = true;
            int reported_depth = depth;
            if (max_depth_limit > 0) {
                reported_depth = std::min(depth, max_depth_limit);
            }
            local.depth_reached = reported_depth;
            flush_thread_node_counter(context);
            publish_best_result(local, shared_result, board, tt, tt_generation, shared,
                                is_primary);
        }

        if (is_primary && shared.has_time_limit) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                iteration_end - shared.start_time);
            if (elapsed >= shared.hard_time_limit) {
                shared.stop.store(true, std::memory_order_relaxed);
                shared.timed_out.store(true, std::memory_order_relaxed);
                local.timed_out = true;
                break;
            }
            auto projected = context.last_iteration_time * 3 / 2;
            if (projected.count() <= 0) {
                projected = std::chrono::milliseconds{15};
            }
            bool enforce_depth_target = best_found && is_major_decision(best_move, root_color) && depth < 12;
            if (elapsed + projected >= shared.soft_time_limit ||
                elapsed >= shared.soft_time_limit) {
                if (enforce_depth_target) {
                    auto extension = std::max(projected, std::chrono::milliseconds{15});
                    auto new_soft = elapsed + extension;
                    if (new_soft > shared.hard_time_limit) {
                        new_soft = shared.hard_time_limit;
                    }
                    if (new_soft > shared.soft_time_limit) {
                        shared.soft_time_limit = new_soft;
                    }
                } else {
                    shared.stop.store(true, std::memory_order_relaxed);
                    break;
                }
            }
        }
    }

    if (infinite_search) {
        while (!shared.stop.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
    }

    if (best_found) {
        local.best_move = best_move;
        local.has_move = true;
    }
    flush_thread_node_counter(context);
    publish_best_result(local, shared_result, board, tt, tt_generation, shared, false);

    return local;
}

SearchResult search_best_move(const Board &board, const SearchLimits &limits) {
    SearchResult result;
    int max_depth_limit = limits.max_depth > 0 ? limits.max_depth : max_search_depth;
    max_depth_limit = std::min(max_depth_limit, max_search_depth);

    SearchSharedState shared;
    shared.start_time = std::chrono::steady_clock::now();

    const bool treat_as_infinite = limits.infinite;

    std::optional<TimeAllocation> allocation;
    if (!treat_as_infinite) {
        allocation = compute_time_allocation(board, limits);
    }
    std::uint64_t nodes_budget_from_time = 0;
    if (allocation.has_value()) {
        shared.has_time_limit = true;
        shared.soft_time_limit = allocation->soft;
        shared.hard_time_limit = allocation->hard;
        if (shared.soft_time_limit.count() <= 0) {
            shared.soft_time_limit = std::chrono::milliseconds{1};
        }
        if (shared.hard_time_limit.count() <= 0) {
            shared.hard_time_limit = shared.soft_time_limit;
        }
        shared.start_time = std::chrono::steady_clock::now();
        nodes_budget_from_time = nodes_budget_for_time(shared.hard_time_limit);
    }

    if (!treat_as_infinite) {
        if (limits.max_nodes > 0) {
            shared.has_node_limit = true;
            shared.node_limit = limits.max_nodes;
            if (nodes_budget_from_time > 0) {
                shared.node_limit = std::min(shared.node_limit, nodes_budget_from_time);
            }
        } else if (nodes_budget_from_time > 0) {
            shared.has_node_limit = true;
            shared.node_limit = nodes_budget_from_time;
        }
    }

    int root_piece_count = total_piece_count(board);
    if (syzygy::available() && root_piece_count <= syzygy::probe_piece_limit() &&
        syzygy::max_pieces() >= root_piece_count) {
        if (auto root_probe = syzygy::probe_root(board); root_probe.has_value()) {
            result.score = syzygy_wdl_to_score(root_probe->wdl, 0);
            if (root_probe->best_move.has_value()) {
                result.best_move = *root_probe->best_move;
                result.has_move = true;
                result.depth_reached = 0;
                if (std::abs(result.score) >= mate_threshold || root_probe->wdl == 0) {
                    return result;
                }
            }
        }
    }

    SearchResult seed = result;

    SharedBestResult shared_result;
    shared_result.result = seed;

    GlobalTranspositionTable &tt = shared_transposition_table();
    std::uint8_t tt_generation = tt.prepare_for_search();

    ActiveSearchGuard active_guard{&shared};

    int thread_count = std::max(1, get_search_threads());
    std::vector<SearchResult> thread_results(static_cast<std::size_t>(thread_count));

    const bool infinite_search = treat_as_infinite && !shared.has_time_limit && !shared.has_node_limit;

    auto worker_fn = [&](int index, bool is_primary) {
        thread_results[static_cast<std::size_t>(index)] = run_search_thread(
            board, max_depth_limit, shared, shared_result, seed, index, is_primary, tt, tt_generation,
            infinite_search);
    };

    SearchThreadPool &pool = SearchThreadPool::instance();
    int background_count = std::max(0, thread_count - 1);
    shared.start_background_tasks(background_count);

    for (int index = 1; index < thread_count; ++index) {
        pool.enqueue(shared, [&, index]() {
            worker_fn(index, false);
        });
    }

    worker_fn(0, true);

    shared.stop.store(true, std::memory_order_relaxed);
    shared.wait_for_background_tasks();

    SearchResult best = shared_result.result;
    for (const auto &candidate : thread_results) {
        if (!candidate.has_move) {
            continue;
        }
        if (!best.has_move || candidate.depth_reached > best.depth_reached ||
            (candidate.depth_reached == best.depth_reached &&
             std::abs(candidate.score) >= std::abs(best.score))) {
            best = candidate;
        }
    }

    if (!best.has_move) {
        auto legal_moves = generate_legal_moves(board);
        if (!legal_moves.empty()) {
            best.best_move = legal_moves.front();
            best.has_move = true;
        }
    }

    best.nodes = shared.node_counter.load(std::memory_order_relaxed);
    if (shared_result.result.seldepth > best.seldepth) {
        best.seldepth = shared_result.result.seldepth;
    }
    if (best.has_move && best.principal_variation.empty()) {
        best.principal_variation = extract_principal_variation(
            board, tt, tt_generation, std::max(best.depth_reached, 1));
        if (best.principal_variation.empty()) {
            best.principal_variation.push_back(best.best_move);
        }
    }
    auto now = std::chrono::steady_clock::now();
    auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - shared.start_time);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_ns);
    NodeThroughputMetrics metrics = compute_node_metrics(shared, best.nodes, elapsed_ns);
    long long elapsed_ms = elapsed.count();
    if (elapsed_ms < 0) {
        elapsed_ms = 0;
    }
    if (elapsed_ms > std::numeric_limits<int>::max()) {
        elapsed_ms = std::numeric_limits<int>::max();
    }
    best.time_ms = static_cast<int>(elapsed_ms);
    best.nodes_per_second = metrics.nps;
    best.knps_before = metrics.knps_before;
    best.knps_after = metrics.knps_after;
    if (shared.timed_out.load(std::memory_order_relaxed)) {
        best.timed_out = true;
    }

    return best;
}

std::string format_uci_score(int score) {
    if (score >= mate_threshold || score <= -mate_threshold) {
        int moves = (mate_score - std::abs(score) + 1) / 2;
        if (score < 0) {
            moves = -moves;
        }
        return std::string{"mate "} + std::to_string(moves);
    }
    return std::string{"cp "} + std::to_string(score);
}

std::string principal_variation_to_uci(const Board &board, const std::vector<Move> &pv) {
    if (pv.empty()) {
        return {};
    }
    Board current = board;
    std::ostringstream stream;
    bool first = true;
    for (const Move &move : pv) {
        auto moves = generate_legal_moves(current);
        auto it = std::find_if(moves.begin(), moves.end(), [&](const Move &candidate) {
            return candidate.from == move.from && candidate.to == move.to &&
                   candidate.piece == move.piece && candidate.captured == move.captured &&
                   candidate.promotion == move.promotion &&
                   candidate.is_en_passant == move.is_en_passant &&
                   candidate.is_castling == move.is_castling;
        });
        if (it == moves.end()) {
            break;
        }
        if (!first) {
            stream << ' ';
        }
        stream << move_to_uci(*it);
        current = current.apply_move(*it);
        first = false;
    }
    return stream.str();
}

void request_stop_search() {
    std::lock_guard<std::mutex> lock(active_search_mutex);
    stop_requested_pending.store(true, std::memory_order_relaxed);
    if (active_search_state != nullptr) {
        active_search_state->stop.store(true, std::memory_order_relaxed);
        stop_requested_pending.store(false, std::memory_order_relaxed);
    }
}

}  // namespace sirio
