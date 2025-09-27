#pragma once
#include "engine/types.hpp"
#include <atomic>
#include <cstdint>
#include <chrono>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

namespace engine {
class Board;

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

    Search();
    Result find_bestmove(Board& b, const Limits& lim);
    void set_threads(int threads);
    void set_hash(int megabytes);
    void stop();
    void set_use_syzygy(bool enable);
    void set_syzygy_path(std::string path);

private:
    struct TTEntry {
        uint64_t key = 0;
        Move move = MOVE_NONE;
        int16_t score = 0;
        int16_t eval = 0;
        int8_t depth = -1;
        uint8_t flag = 0;
    };
    struct ThreadData;

    Result search_position(Board& board, const Limits& lim);
    int negamax(Board& board, int depth, int alpha, int beta, bool pv_node, int ply,
                ThreadData& thread_data);
    int quiescence(Board& board, int alpha, int beta, int ply, ThreadData& thread_data);
    void store_tt(uint64_t key, Move best, int depth, int score, int flag, int ply,
                  int eval);
    bool probe_tt(const Board& board, int depth, int alpha, int beta, Move& tt_move,
                  int& score, int ply) const;
    std::vector<Move> order_moves(const Board& board, std::vector<Move>& moves,
                                  Move tt_move, int ply, const ThreadData& thread_data) const;
    void update_killers(ThreadData& thread_data, int ply, Move move);
    std::vector<Move> extract_pv(const Board& board, Move best) const;
    int evaluate(const Board& board) const;
    std::optional<int> probe_syzygy(const Board& board) const;

    std::vector<TTEntry> tt_;
    size_t tt_mask_ = 0;
    mutable std::shared_mutex tt_mutex_;
    std::atomic<uint64_t> nodes_;
    std::atomic<bool> stop_;
    int threads_ = 1;
    bool use_syzygy_ = false;
    std::string syzygy_path_;
    std::optional<std::chrono::steady_clock::time_point> deadline_;
};

} // namespace engine
