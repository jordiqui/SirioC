#pragma once
#include "engine/syzygy/syzygy.hpp"
#include "engine/types.hpp"
#include "engine/util/time.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>
#include <mutex>

#include "tt.h"

namespace engine {
class Board;
namespace nnue {
class Evaluator;
}

class Search {
public:
    struct Result {
        Move bestmove = MOVE_NONE;
        int depth = 0;
        int score = 0;
        uint64_t nodes = 0;
        int time_ms = 0;
        bool is_mate = false;
        std::vector<Move> pv;
    };

    struct Info {
        int depth = 0;
        int score = 0;
        uint64_t nodes = 0;
        int time_ms = 0;
        std::vector<Move> pv;
    };

    Search();
    Result find_bestmove(Board& b, const Limits& lim);
    void set_info_callback(std::function<void(const Info&)> cb);
    void set_threads(int threads);
    void set_hash(int megabytes);
    void stop();
    void set_syzygy_config(syzygy::TBConfig config);
    void set_numa_offset(int offset);
    void set_ponder(bool enable);
    void set_multi_pv(int multi_pv);
    void set_move_overhead(int overhead_ms);
    void set_time_config(time::TimeConfig config);
    void set_eval_file(std::string path);
    void set_eval_file_small(std::string path);
    void set_nnue_evaluator(const nnue::Evaluator* evaluator);
    void set_use_nnue(bool enable);

private:
    struct ThreadData {
        ThreadData();
        void reset();

        static constexpr int kMaxPly = 128;

        struct QuiescenceCapture {
            Move move = MOVE_NONE;
            int see = 0;
        };

        std::vector<std::array<Move, 2>> killers;
        std::array<int, 64 * 64> history{};
        std::array<Move, 64 * 64> countermoves{};
        std::vector<QuiescenceCapture> quiescence_captures;
        std::vector<Move> quiescence_checks;
        size_t id = 0;
    };
    struct AdaptiveTuning {
        void reset();
        void prepare(int threads, int64_t target_time_ms);
        void begin_iteration(uint64_t nodes,
                             std::chrono::steady_clock::time_point start_time);
        void end_iteration(uint64_t nodes,
                           std::chrono::steady_clock::time_point end_time);

        int futility_margin(int depth) const;
        int reverse_futility_margin() const;
        int razoring_margin() const;
        int late_move_limit(int depth, int move_overhead_ms, int history_score,
                            int move_count) const;
        int beta_margin(int depth) const;
        int probcut_margin() const;
        int singular_margin_scale() const;
        int singular_reduction_base() const;
        int null_move_base_reduction() const;
        int null_move_max_reduction() const;
        int null_move_depth_divisor() const;
        double lmr_scale() const;

    private:
        void apply_scaling();

        std::array<int, 4> futility_margins_{};
        std::array<int, 7> late_move_limits_{};
        std::array<int, 7> beta_margins_{};
        int probcut_margin_ = 0;
        int singular_margin_scale_ = 0;
        int singular_reduction_base_ = 0;
        int null_move_base_reduction_ = 0;
        int null_move_max_reduction_ = 0;
        int null_move_depth_divisor_ = 4;
        int reverse_futility_margin_ = 0;
        int razoring_margin_ = 0;
        double lmr_scale_ = 1.0;

        int threads_ = 1;
        int64_t target_time_ms_ = -1;
        bool has_baseline_speed_ = false;
        double baseline_nodes_per_ms_ = 1.0;
        double speed_ema_ = 1.0;
        uint64_t iteration_nodes_start_ = 0;
        std::chrono::steady_clock::time_point iteration_start_{};
    };

    Result search_position(Board& board, const Limits& lim);
    int negamax(Board& board, int depth, int alpha, int beta, bool pv_node, int ply,
                ThreadData& thread_data, Move prev_move, bool in_iid = false);
    int quiescence(Board& board, int alpha, int beta, int ply, ThreadData& thread_data);
    void store_tt(uint64_t key, Move best, int depth, int score, int flag, int ply,
                  int eval);
    bool probe_tt(const Board& board, int depth, int alpha, int beta, Move& tt_move,
                  int& score, int ply, int& tt_depth, int& tt_flag, int& tt_eval) const;
    std::vector<Move> order_moves(const Board& board, std::vector<Move>& moves,
                                  Move tt_move, int ply, const ThreadData& thread_data,
                                  Move prev_move) const;
    void update_killers(ThreadData& thread_data, int ply, Move move);
    void update_history(ThreadData& thread_data, Move move, int delta);
    std::vector<Move> extract_pv(const Board& board, Move best) const;
    int evaluate(const Board& board) const;
    std::optional<int> probe_syzygy(const Board& board, int depth,
                                    bool root_probe) const;

    TranspositionTable tt_;
    std::atomic<bool> stop_;
    std::vector<ThreadData> thread_data_pool_;
    uint64_t thread_data_position_key_ = 0;
    size_t thread_data_thread_count_ = 0;
    bool thread_data_initialized_ = false;
    int threads_ = 1;
    void start_worker_threads(size_t thread_count);
    void stop_worker_threads();
    void submit_task(std::function<void(ThreadData&)> task);
    bool run_available_task(ThreadData& main_td);
    void wait_for_all_tasks(ThreadData& main_td);
    void worker_loop(size_t index);
    std::vector<std::thread> worker_threads_;
    std::deque<std::function<void(ThreadData&)>> task_queue_;
    std::mutex task_mutex_;
    std::condition_variable task_cv_;
    std::condition_variable task_done_cv_;
    std::atomic<bool> pool_stop_{false};
    std::atomic<size_t> pending_tasks_{0};
    syzygy::TBConfig syzygy_config_{};
    int numa_offset_ = 0;
    bool ponder_ = true;
    int multi_pv_ = 1;
    int move_overhead_ms_ = 10;
    time::TimeConfig time_config_{};
    std::string eval_file_ = "nn-1c0000000000.nnue";
    std::string eval_file_small_ = "nn-37f18f62d772.nnue";
    std::optional<std::chrono::steady_clock::time_point> deadline_;
    const nnue::Evaluator* nnue_eval_ = nullptr;
    bool use_nnue_eval_ = false;
    int64_t target_time_ms_ = -1;
    int64_t nodes_limit_ = -1;
    std::chrono::steady_clock::time_point search_start_;
    std::function<void(const Info&)> info_callback_;
    AdaptiveTuning tuning_;
};

} // namespace engine
