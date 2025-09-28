#include "engine/search/search.hpp"

#include "engine/core/board.hpp"
#include "engine/eval/eval.hpp"
#include "engine/eval/nnue/evaluator.hpp"
#include "engine/syzygy/syzygy.hpp"
#include "engine/util/time.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <mutex>
#include <sstream>
#include <thread>
#include <atomic>
#include <vector>
#include <optional>

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
constexpr int kAspirationWindow = 25;
constexpr int kNullMoveBaseReduction = 2;
constexpr int kMaxNullMoveReduction = 3;
constexpr int kNullMoveDepthDivisor = 4;
constexpr int kReverseFutilityMargin = 200;
constexpr int kRazoringMargin = 400;
constexpr int kProbCutMargin = 200;
constexpr int kSingularMarginScale = 32;
constexpr int kSingularReductionBase = 2;
constexpr std::array<int, 7> kLateMovePruningLimits{0, 3, 6, 10, 14, 18, 24};
constexpr std::array<int, 7> kEnhancedBetaMargins{0, 90, 140, 200, 260, 340, 420};

bool has_non_pawn_material(const Board& board, bool white) {
    const auto& bb = board.piece_bitboards();
    if (white) {
        for (int idx = Board::WHITE_KNIGHT; idx <= Board::WHITE_QUEEN; ++idx) {
            if (bb[static_cast<size_t>(idx)] != 0ULL) return true;
        }
    } else {
        for (int idx = Board::BLACK_KNIGHT; idx <= Board::BLACK_QUEEN; ++idx) {
            if (bb[static_cast<size_t>(idx)] != 0ULL) return true;
        }
    }
    return false;
}

int static_exchange_eval(const Board& board, Move move) {
    char captured = board.piece_on(move_to(move));
    if (move_is_enpassant(move)) {
        captured = board.white_to_move() ? 'p' : 'P';
    }
    if (captured == '.') return 0;
    char mover = board.piece_on(move_from(move));
    int gain = piece_value(captured) - piece_value(mover);
    return gain;
}

std::string wdl_to_string(syzygy::WdlOutcome outcome) {
    switch (outcome) {
    case syzygy::WdlOutcome::Win: return "win";
    case syzygy::WdlOutcome::CursedWin: return "cursed-win";
    case syzygy::WdlOutcome::Draw: return "draw";
    case syzygy::WdlOutcome::BlessedLoss: return "blessed-loss";
    case syzygy::WdlOutcome::Loss: return "loss";
    }
    return "unknown";
}

int tablebase_score(const syzygy::TB::ProbeResult& probe,
                    const syzygy::TBConfig& config) {
    auto mate_score = [](bool winning, int distance) {
        int d = std::max(1, distance);
        int capped = std::min(kMateValue - 1, d);
        int score = kMateValue - capped;
        return winning ? score : -score;
    };

    switch (probe.wdl) {
    case syzygy::WdlOutcome::Win: {
        int dist = probe.dtz ? std::max(1, std::abs(*probe.dtz)) : 1;
        return mate_score(true, dist);
    }
    case syzygy::WdlOutcome::Loss: {
        int dist = probe.dtz ? std::max(1, std::abs(*probe.dtz)) : 1;
        return mate_score(false, dist);
    }
    case syzygy::WdlOutcome::Draw:
        return 0;
    case syzygy::WdlOutcome::CursedWin:
        if (config.use_rule50) return 0;
        return mate_score(true, probe.dtz ? std::max(1, std::abs(*probe.dtz)) : 1);
    case syzygy::WdlOutcome::BlessedLoss:
        if (config.use_rule50) return 0;
        return mate_score(false, probe.dtz ? std::max(1, std::abs(*probe.dtz)) : 1);
    }
    return 0;
}

} // namespace

Search::ThreadData::ThreadData() { reset(); }

void Search::ThreadData::reset() {
    killers.assign(kMaxPly, {MOVE_NONE, MOVE_NONE});
    history.fill(0);
    countermoves.fill(MOVE_NONE);
}

void Search::AdaptiveTuning::reset() {
    futility_margins_ = kFutilityMargins;
    late_move_limits_ = kLateMovePruningLimits;
    beta_margins_ = kEnhancedBetaMargins;
    probcut_margin_ = kProbCutMargin;
    singular_margin_scale_ = kSingularMarginScale;
    singular_reduction_base_ = kSingularReductionBase;
    null_move_base_reduction_ = kNullMoveBaseReduction;
    null_move_max_reduction_ = kMaxNullMoveReduction;
    null_move_depth_divisor_ = kNullMoveDepthDivisor;
    reverse_futility_margin_ = kReverseFutilityMargin;
    razoring_margin_ = kRazoringMargin;
    lmr_scale_ = 1.0;
    threads_ = 1;
    target_time_ms_ = -1;
    has_baseline_speed_ = false;
    baseline_nodes_per_ms_ = 1.0;
    speed_ema_ = 1.0;
    iteration_nodes_start_ = 0;
    iteration_start_ = {};
}

void Search::AdaptiveTuning::prepare(int threads, int64_t target_time_ms) {
    threads_ = std::max(1, threads);
    target_time_ms_ = target_time_ms;
    has_baseline_speed_ = false;
    baseline_nodes_per_ms_ = 1.0;
    speed_ema_ = 1.0;
    apply_scaling();
}

void Search::AdaptiveTuning::begin_iteration(
    uint64_t nodes, std::chrono::steady_clock::time_point start_time) {
    iteration_nodes_start_ = nodes;
    iteration_start_ = start_time;
}

void Search::AdaptiveTuning::end_iteration(
    uint64_t nodes, std::chrono::steady_clock::time_point end_time) {
    if (iteration_nodes_start_ > nodes) {
        iteration_nodes_start_ = nodes;
    }
    auto elapsed = std::chrono::duration<double, std::milli>(end_time - iteration_start_).count();
    if (elapsed <= 0.0) {
        return;
    }
    double explored = static_cast<double>(nodes - iteration_nodes_start_);
    if (explored <= 0.0) {
        return;
    }
    double nodes_per_ms = explored / elapsed;
    if (!has_baseline_speed_) {
        baseline_nodes_per_ms_ = std::max(1e-6, nodes_per_ms);
        has_baseline_speed_ = true;
        speed_ema_ = 1.0;
    } else {
        double ratio = nodes_per_ms / std::max(1e-6, baseline_nodes_per_ms_);
        speed_ema_ = 0.85 * speed_ema_ + 0.15 * std::clamp(ratio, 0.2, 5.0);
    }
    apply_scaling();
}

int Search::AdaptiveTuning::futility_margin(int depth) const {
    size_t idx = static_cast<size_t>(std::min(depth, static_cast<int>(futility_margins_.size() - 1)));
    return futility_margins_[idx];
}

int Search::AdaptiveTuning::reverse_futility_margin() const { return reverse_futility_margin_; }

int Search::AdaptiveTuning::razoring_margin() const { return razoring_margin_; }

int Search::AdaptiveTuning::late_move_limit(int depth, int move_overhead_ms, int history_score,
                                            int move_count) const {
    size_t idx = static_cast<size_t>(std::min(depth, static_cast<int>(late_move_limits_.size() - 1)));
    int limit = late_move_limits_[idx] + move_overhead_ms / 40;
    if (history_score < 0) {
        --limit;
        if (move_count > limit) {
            --limit;
        }
    }
    return std::max(0, limit);
}

int Search::AdaptiveTuning::beta_margin(int depth) const {
    size_t idx = static_cast<size_t>(std::min(depth, static_cast<int>(beta_margins_.size() - 1)));
    return beta_margins_[idx];
}

int Search::AdaptiveTuning::probcut_margin() const { return probcut_margin_; }

int Search::AdaptiveTuning::singular_margin_scale() const { return singular_margin_scale_; }

int Search::AdaptiveTuning::singular_reduction_base() const { return singular_reduction_base_; }

int Search::AdaptiveTuning::null_move_base_reduction() const { return null_move_base_reduction_; }

int Search::AdaptiveTuning::null_move_max_reduction() const { return null_move_max_reduction_; }

int Search::AdaptiveTuning::null_move_depth_divisor() const { return null_move_depth_divisor_; }

double Search::AdaptiveTuning::lmr_scale() const { return lmr_scale_; }

void Search::AdaptiveTuning::apply_scaling() {
    futility_margins_ = kFutilityMargins;
    late_move_limits_ = kLateMovePruningLimits;
    beta_margins_ = kEnhancedBetaMargins;
    probcut_margin_ = kProbCutMargin;
    singular_margin_scale_ = kSingularMarginScale;
    singular_reduction_base_ = kSingularReductionBase;
    null_move_base_reduction_ = kNullMoveBaseReduction;
    null_move_max_reduction_ = kMaxNullMoveReduction;
    null_move_depth_divisor_ = kNullMoveDepthDivisor;
    reverse_futility_margin_ = kReverseFutilityMargin;
    razoring_margin_ = kRazoringMargin;
    lmr_scale_ = 1.0;

    double hardware_scale = std::clamp(std::sqrt(static_cast<double>(threads_)), 1.0, 2.0);
    double time_scale = 1.0;
    if (target_time_ms_ > 0) {
        time_scale = std::clamp(std::sqrt(static_cast<double>(target_time_ms_) / 1000.0), 0.6, 2.0);
    }
    double speed_scale = std::clamp(speed_ema_, 0.5, 1.8);
    double effective = std::clamp(hardware_scale * time_scale * speed_scale, 0.6, 1.8);
    double margin_scale = std::clamp(1.0 / std::sqrt(effective), 0.7, 1.3);
    double pruning_scale = std::clamp(std::sqrt(effective), 0.8, 1.25);
    double extension_scale = std::clamp(1.0 / pruning_scale, 0.75, 1.25);
    lmr_scale_ = std::clamp(extension_scale, 0.75, 1.25);

    for (size_t i = 0; i < futility_margins_.size(); ++i) {
        futility_margins_[i] = std::max(0, static_cast<int>(std::round(kFutilityMargins[i] * margin_scale)));
    }
    for (size_t i = 0; i < late_move_limits_.size(); ++i) {
        late_move_limits_[i] = std::max(0, static_cast<int>(std::round(kLateMovePruningLimits[i] * pruning_scale)));
    }
    for (size_t i = 0; i < beta_margins_.size(); ++i) {
        beta_margins_[i] = std::max(50, static_cast<int>(std::round(kEnhancedBetaMargins[i] * margin_scale)));
    }

    probcut_margin_ = std::max(80, static_cast<int>(std::round(kProbCutMargin * margin_scale)));
    singular_margin_scale_ = std::max(16, static_cast<int>(std::round(kSingularMarginScale * margin_scale)));
    singular_reduction_base_ = std::clamp(static_cast<int>(std::round(kSingularReductionBase * extension_scale)), 1, 4);
    null_move_base_reduction_ = std::clamp(static_cast<int>(std::round(kNullMoveBaseReduction * extension_scale)), 1, 4);
    null_move_max_reduction_ = std::clamp(static_cast<int>(std::round(kMaxNullMoveReduction * extension_scale)), 2, 5);
    null_move_depth_divisor_ = std::clamp(static_cast<int>(std::round(kNullMoveDepthDivisor / std::clamp(effective, 0.7, 1.6))), 3, 6);
    reverse_futility_margin_ = std::max(80, static_cast<int>(std::round(kReverseFutilityMargin * margin_scale)));
    razoring_margin_ = std::max(200, static_cast<int>(std::round(kRazoringMargin * margin_scale)));
}

Search::Search() : nodes_(0), stop_(false) {
    set_hash(16);
    target_time_ms_ = -1;
    nodes_limit_ = -1;
    tuning_.reset();
}

void Search::set_info_callback(std::function<void(const Info&)> cb) {
    info_callback_ = std::move(cb);
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

void Search::set_syzygy_config(syzygy::TBConfig config) {
    syzygy_config_ = std::move(config);
    if (!syzygy_config_.enabled || syzygy_config_.path.empty()) {
        syzygy::shutdown();
    } else {
        syzygy::configure(syzygy_config_);
    }
}

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

    size_t thread_count = static_cast<size_t>(std::max(1, threads_));
    bool thread_count_changed = thread_data_thread_count_ != thread_count;
    uint64_t position_key = board.zobrist_key();
    bool new_position = !thread_data_initialized_ || thread_data_position_key_ != position_key;
    thread_data_pool_.resize(thread_count);
    if (thread_count_changed || new_position) {
        for (auto& td : thread_data_pool_) {
            td.reset();
        }
    }
    thread_data_thread_count_ = thread_count;
    thread_data_position_key_ = position_key;
    thread_data_initialized_ = true;

    auto alloc = time::compute_allocation(lim, board.white_to_move(), move_overhead_ms_);
    target_time_ms_ = alloc.optimal_ms;
    if (alloc.maximum_ms > 0) {
        deadline_ = start + std::chrono::milliseconds(alloc.maximum_ms);
    } else {
        deadline_.reset();
    }
    nodes_limit_ = lim.nodes >= 0 ? lim.nodes : -1;

    tuning_.prepare(threads_, target_time_ms_);

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
    result.bestmove = best_move;
    result.depth = 0;
    result.score = best_score;
    result.is_mate = false;

    int depth_limit = lim.depth > 0 ? lim.depth : 64;
    depth_limit = std::min(depth_limit, 64);

    int tb_limit = syzygy_config_.probe_limit;
    if (syzygy_config_.enabled && !syzygy_config_.path.empty() &&
        depth_limit >= syzygy_config_.probe_depth &&
        (tb_limit < 0 || syzygy::TB::pieceCount(board) <= tb_limit)) {
        if (auto tb = syzygy::TB::probePosition(board, syzygy_config_, true)) {
            int tb_score = tablebase_score(*tb, syzygy_config_);
            result.bestmove = tb->best_move.value_or(MOVE_NONE);
            result.depth = depth_limit;
            result.score = tb_score;
            result.is_mate = std::abs(tb_score) >= kMateThreshold;
            result.nodes = nodes_.load(std::memory_order_relaxed);
            result.time_ms = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start)
                    .count());
            result.pv.clear();
            if (tb->best_move) {
                result.pv.push_back(*tb->best_move);
            }

            std::ostringstream oss;
            oss << "info string Syzygy hit wdl " << wdl_to_string(tb->wdl);
            if (tb->dtz) {
                oss << " dtz " << *tb->dtz;
            }
            if (tb->best_move) {
                oss << " move " << board.move_to_uci(*tb->best_move);
            }
            std::cout << oss.str() << '\n' << std::flush;

            deadline_.reset();
            target_time_ms_ = -1;
            nodes_limit_ = -1;
            return result;
        }
    }

    int aspiration_delta = kAspirationWindow;
    int fail_high_streak = 0;
    int fail_low_streak = 0;

    for (int depth = 1; depth <= depth_limit; ++depth) {
        if (stop_.load(std::memory_order_relaxed)) break;

        tuning_.begin_iteration(nodes_.load(std::memory_order_relaxed),
                                std::chrono::steady_clock::now());

        int alpha_window = -kInfiniteScore;
        int beta_window = kInfiniteScore;
        int local_delta = std::max(kAspirationWindow,
                                   aspiration_delta + fail_high_streak * 5 +
                                       fail_low_streak * 5);
        if (depth > 1 && result.depth > 0) {
            alpha_window = std::max(-kInfiniteScore, best_score - local_delta);
            beta_window = std::min(kInfiniteScore, best_score + local_delta);
        }

        std::vector<std::pair<Move, int>> best_scores;

        while (!stop_.load(std::memory_order_relaxed)) {
            int search_alpha = alpha_window;
            int search_beta = beta_window;
            std::vector<std::optional<std::pair<Move, int>>> scores(root_moves.size());
            std::atomic<size_t> next_index{0};
            std::atomic<size_t> completed{0};

            auto worker = [&](ThreadData& td) {
                Board local_board = board;
                while (!stop_.load(std::memory_order_relaxed)) {
                    size_t idx = next_index.fetch_add(1, std::memory_order_relaxed);
                    if (idx >= root_moves.size()) break;
                    Move move = root_moves[idx];
                    Board::State state;
                    local_board.apply_move(move, state);
                    int score = -negamax(local_board, depth - 1, -search_beta, -search_alpha, true, 1, td, move, false);
                    local_board.undo_move(state);
                    scores[idx] = std::make_pair(move, score);
                    completed.fetch_add(1, std::memory_order_relaxed);
                }
            };

            std::vector<std::thread> workers;
            workers.reserve(thread_count > 0 ? thread_count - 1 : 0);
            for (size_t t = 1; t < thread_count; ++t) {
                workers.emplace_back(worker, std::ref(thread_data_pool_[t]));
            }
            worker(thread_data_pool_[0]);
            for (auto& th : workers) th.join();

            if (stop_.load(std::memory_order_relaxed)) break;

            size_t finished = std::min(completed.load(std::memory_order_relaxed), scores.size());
            std::vector<std::pair<Move, int>> filtered_scores;
            filtered_scores.reserve(finished);
            for (auto& entry : scores) {
                if (entry && entry->first != MOVE_NONE) {
                    filtered_scores.push_back(*entry);
                }
            }

            if (filtered_scores.empty()) {
                break;
            }

            std::sort(filtered_scores.begin(), filtered_scores.end(), [](const auto& lhs, const auto& rhs) {
                return lhs.second > rhs.second;
            });

            if (!filtered_scores.empty()) {
                best_scores = filtered_scores;
                best_move = filtered_scores.front().first;
                best_score = filtered_scores.front().second;
                bool failed_low = best_score <= search_alpha;
                bool failed_high = best_score >= search_beta;

                if (failed_low) {
                    fail_low_streak++;
                    fail_high_streak = 0;
                    local_delta = std::min(400, local_delta * 2 + 5);
                    alpha_window = std::max(-kInfiniteScore, best_score - local_delta);
                    beta_window = search_beta;
                    if (alpha_window <= -kInfiniteScore + 1 || search_alpha == -kInfiniteScore) {
                        alpha_window = -kInfiniteScore;
                        beta_window = search_beta;
                        break;
                    }
                    continue;
                }
                if (failed_high) {
                    fail_high_streak++;
                    fail_low_streak = 0;
                    local_delta = std::min(400, local_delta * 2 + 5);
                    beta_window = std::min(kInfiniteScore, best_score + local_delta);
                    alpha_window = search_alpha;
                    if (beta_window >= kInfiniteScore - 1 || search_beta == kInfiniteScore) {
                        beta_window = kInfiniteScore;
                        alpha_window = search_alpha;
                        break;
                    }
                    continue;
                }

                result.bestmove = best_move;
                result.depth = depth;
                result.score = best_score;
                result.is_mate = std::abs(best_score) >= kMateThreshold;

                aspiration_delta = std::max(kAspirationWindow / 2, local_delta / 2);
                fail_high_streak = 0;
                fail_low_streak = 0;

                if (info_callback_) {
                    Info info;
                    info.depth = depth;
                    info.score = best_score;
                    info.nodes = nodes_.load(std::memory_order_relaxed);
                    info.time_ms = static_cast<int>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start)
                            .count());
                    info.pv = extract_pv(board, best_move);
                    info_callback_(info);
                }
                break;
            }
        }

        if (!best_scores.empty()) {
            root_moves.clear();
            for (const auto& [move, score] : best_scores) {
                root_moves.push_back(move);
            }
        }

        auto now = std::chrono::steady_clock::now();
        tuning_.end_iteration(nodes_.load(std::memory_order_relaxed), now);
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
                    ThreadData& thread_data, Move prev_move, bool in_iid) {
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

    if (depth <= 0) { return quiescence(board, alpha, beta, ply, thread_data); }

    if (auto tb = probe_syzygy(board, depth, false)) return *tb;

    int static_eval = evaluate(board);

    Move tt_move = MOVE_NONE;
    int tt_score = static_eval;
    int tt_depth = -1;
    int tt_flag = TT_EXACT;
    int tt_eval = static_eval;
    if (probe_tt(board, depth, alpha, beta, tt_move, tt_score, ply, tt_depth, tt_flag, tt_eval)) {
        return tt_score;
    }
    static_eval = tt_eval;

    if (!pv_node && depth <= 3 && !in_check) {
        int margin = static_eval - tuning_.reverse_futility_margin() * depth;
        if (margin >= beta) return margin;
    }

    if (!pv_node && depth <= 3 && !in_check &&
        static_eval + tuning_.razoring_margin() <= alpha) {
        int razor = quiescence(board, alpha - 1, alpha, ply, thread_data);
        if (razor <= alpha) return razor;
    }

    if (!pv_node && depth >= 2 && !in_check && static_eval >= beta &&
        has_non_pawn_material(board, board.white_to_move())) {
        Board::State null_state;
        board.apply_null_move(null_state);
        int reduction = tuning_.null_move_base_reduction();
        int extra = depth / std::max(1, tuning_.null_move_depth_divisor());
        reduction += std::min(tuning_.null_move_max_reduction(), extra);
        int score = -negamax(board, depth - 1 - reduction, -beta, -beta + 1, false, ply + 1,
                              thread_data, MOVE_NONE, false);
        board.undo_move(null_state);
        if (stop_.load(std::memory_order_relaxed)) return beta;
        if (score >= beta) {
            return score;
        }
    }

    if (!pv_node && !in_check) {
        int margin = tuning_.beta_margin(depth);
        if (static_eval - margin >= beta) {
            return static_eval - margin;
        }
    }

    if (!in_iid && depth >= 6 && !in_check && tt_move == MOVE_NONE &&
        std::abs(beta) < kMateThreshold) {
        int iid_depth = depth - 2;
        int iid_beta = std::min(beta, alpha + 1);
        int iid_score = negamax(board, iid_depth, alpha, iid_beta, pv_node, ply, thread_data,
                                 prev_move, true);
        if (stop_.load(std::memory_order_relaxed)) {
            return iid_score;
        }
        probe_tt(board, depth, alpha, beta, tt_move, tt_score, ply, tt_depth, tt_flag, tt_eval);
        static_eval = tt_eval;
    }

    auto moves = board.generate_legal_moves();
    if (moves.empty()) {
        if (board.side_to_move_in_check()) return -kMateValue + ply;
        return 0;
    }

    if (!pv_node && depth >= 5 && !in_check && std::abs(beta) < kMateThreshold) {
        int probcut_beta = beta + tuning_.probcut_margin();
        int probcut_alpha = probcut_beta - 1;
        int probcut_depth = depth - 3;
        for (Move move : moves) {
            if (!move_is_capture(move) && move_promo(move) == 0) continue;
            if (static_exchange_eval(board, move) < -80) continue;
            Board::State pc_state;
            board.apply_move(move, pc_state);
            int score = -negamax(board, probcut_depth, -probcut_beta, -probcut_alpha, false,
                                  ply + 1, thread_data, move, false);
            board.undo_move(pc_state);
            if (stop_.load(std::memory_order_relaxed)) return probcut_beta;
            if (score >= probcut_beta) return score;
        }
    }

    auto ordered = order_moves(board, moves, tt_move, ply, thread_data, prev_move);

    int best_score = -kInfiniteScore;
    Move best_move = MOVE_NONE;
    int alpha_orig = alpha;
    bool first = true;
    int move_count = 0;

    bool singular_ready = (!in_check && tt_move != MOVE_NONE && tt_flag == TT_LOWER &&
                           tt_depth >= depth - 1 && depth >= 4 &&
                           std::abs(tt_score) < kMateThreshold);
    int singular_beta = tt_score - tuning_.singular_margin_scale() * depth;
    int singular_reduction = tuning_.singular_reduction_base() +
                             depth / std::max(1, tuning_.null_move_depth_divisor());
    int singular_depth = depth - 1 - singular_reduction;
    bool allow_singular = singular_ready && singular_depth >= 1 && singular_beta > alpha;

    for (Move move : ordered) {
        ++move_count;
        int prev_alpha = alpha;
        bool is_capture = move_is_capture(move);
        bool is_promo = move_promo(move) != 0;
        bool quiet = !is_capture && !is_promo;
        int history_score = thread_data.history[static_cast<size_t>(history_index(move))];

        if (!pv_node && is_capture && depth <= 1 && static_exchange_eval(board, move) < 0) {
            continue;
        }

        int extension = 0;
        if (allow_singular && move == tt_move) {
            int best_other = singular_beta - 1;
            for (Move alt : moves) {
                if (alt == move) continue;
                Board::State singular_state;
                board.apply_move(alt, singular_state);
                int score = -negamax(board, singular_depth, -singular_beta, -best_other, false,
                                      ply + 1, thread_data, alt, false);
                board.undo_move(singular_state);
                if (stop_.load(std::memory_order_relaxed)) break;
                if (score >= singular_beta) {
                    best_other = score;
                    break;
                }
                best_other = std::max(best_other, score);
            }
            if (stop_.load(std::memory_order_relaxed)) return alpha;
            if (best_other < singular_beta) {
                extension = 1;
            }
        }

        Board::State state;
        board.apply_move(move, state);
        bool gives_check = board.side_to_move_in_check();

        if (!in_check && quiet && depth <= 3 && !gives_check) {
            int margin = tuning_.futility_margin(depth);
            if (static_eval + margin <= alpha) {
                board.undo_move(state);
                continue;
            }
        }

        if (!pv_node && quiet && !in_check && !gives_check) {
            int lmp_limit = tuning_.late_move_limit(depth, move_overhead_ms_, history_score, move_count);
            if (move_count > std::max(1, lmp_limit)) {
                board.undo_move(state);
                continue;
            }
        }

        if (!pv_node && quiet && depth <= 3 && move_count > 4 + depth && history_score < 0) {
            board.undo_move(state);
            continue;
        }

        int score;
        int child_depth = std::max(0, depth - 1 + extension);
        if (first) {
            score = -negamax(board, child_depth, -beta, -alpha, pv_node, ply + 1, thread_data,
                              move, false);
            first = false;
        } else {
            int new_depth = child_depth;
            bool apply_lmr = new_depth > 0 && depth >= 3 && move_count > 3 && quiet &&
                             !gives_check && !pv_node;
            if (apply_lmr) {
                double reduction_value = 1.0 + (depth > 4) + (move_count > 4) + (move_count > 8);
                if (history_score < 0) {
                    reduction_value += 1.0;
                }
                reduction_value *= tuning_.lmr_scale();
                reduction_value += (move_overhead_ms_ / 60.0) * tuning_.lmr_scale();
                int reduction = std::min(new_depth, std::max(1, static_cast<int>(std::round(reduction_value))));
                int reduced_depth = std::max(1, new_depth - reduction);
                score = -negamax(board, reduced_depth, -alpha - 1, -alpha, false, ply + 1,
                                  thread_data, move, false);
                if (score > alpha) {
                    score = -negamax(board, new_depth, -alpha - 1, -alpha, false, ply + 1,
                                      thread_data, move, false);
                }
            } else {
                score = -negamax(board, new_depth, -alpha - 1, -alpha, false, ply + 1,
                                  thread_data, move, false);
            }
            if (score > alpha && score < beta) {
                score = -negamax(board, new_depth, -beta, -alpha, true, ply + 1, thread_data,
                                  move, false);
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
                if (prev_move != MOVE_NONE) {
                    thread_data.countermoves[static_cast<size_t>(history_index(prev_move))] = move;
                }
            }
            best_score = alpha;
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
                      int& score, int ply, int& tt_depth, int& tt_flag, int& tt_eval) const {
    if (tt_.empty()) {
        tt_move = MOVE_NONE;
        tt_depth = -1;
        tt_flag = TT_EXACT;
        return false;
    }
    uint64_t key = board.zobrist_key();
    const TTEntry* entry = nullptr;
    {
        std::shared_lock lock(tt_mutex_);
        TTEntry const& slot = tt_[key & tt_mask_];
        if (slot.key == key) entry = &slot;
    }
    if (!entry) {
        tt_move = MOVE_NONE;
        tt_depth = -1;
        tt_flag = TT_EXACT;
        return false;
    }

    tt_move = entry->move;
    score = entry->score;
    tt_depth = entry->depth;
    tt_flag = entry->flag;
    tt_eval = entry->eval;
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
                                      int ply, const ThreadData& thread_data, Move prev_move) const {
    std::vector<std::pair<int, Move>> scored;
    scored.reserve(moves.size());
    Move counter = MOVE_NONE;
    if (prev_move != MOVE_NONE) {
        counter = thread_data.countermoves[static_cast<size_t>(history_index(prev_move))];
    }
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
            if (score == 0 && counter != MOVE_NONE && move == counter) {
                score = 298'500;
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
    Board current = board;
    std::vector<Board::State> states;
    states.reserve(kMaxPly);
    Board::State state;
    current.apply_move(best, state);
    states.push_back(state);
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
        Board::State next_state;
        current.apply_move(next, next_state);
        states.push_back(next_state);
    }
    for (auto it = states.rbegin(); it != states.rend(); ++it) {
        current.undo_move(*it);
    }
    return pv;
}

int Search::evaluate(const Board& board) const {
    if (use_nnue_eval_ && nnue_eval_) {
        return nnue_eval_->eval_cp(board);
    }
    return eval::evaluate(board);
}

std::optional<int> Search::probe_syzygy(const Board& board, int depth,
                                        bool root_probe) const {
    if (!syzygy_config_.enabled || syzygy_config_.path.empty()) {
        return std::nullopt;
    }
    if (depth < syzygy_config_.probe_depth) {
        return std::nullopt;
    }
    if (syzygy_config_.probe_limit >= 0 &&
        syzygy::TB::pieceCount(board) > syzygy_config_.probe_limit) {
        return std::nullopt;
    }

    auto probe = syzygy::TB::probePosition(board, syzygy_config_, root_probe);
    if (!probe) return std::nullopt;
    return tablebase_score(*probe, syzygy_config_);
}

} // namespace engine
