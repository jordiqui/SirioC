#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "sirio/board.hpp"
#include "sirio/move.hpp"

namespace sirio {

struct SearchLimits {
    int max_depth = 0;
    int move_time = 0;
    int time_left_white = 0;
    int time_left_black = 0;
    int increment_white = 0;
    int increment_black = 0;
    int moves_to_go = 0;
    std::uint64_t max_nodes = 0;
};

struct SearchResult {
    Move best_move;
    int score = 0;
    bool has_move = false;
    int depth_reached = 0;
    bool timed_out = false;
    std::uint64_t nodes = 0;
};

SearchResult search_best_move(const Board &board, const SearchLimits &limits);

void set_search_threads(int threads);
int get_search_threads();

void set_transposition_table_size(std::size_t size_mb);
std::size_t get_transposition_table_size();
void clear_transposition_tables();
bool save_transposition_table(const std::string &path, std::string *error = nullptr);
bool load_transposition_table(const std::string &path, std::string *error = nullptr);

void set_move_overhead(int milliseconds);
void set_minimum_thinking_time(int milliseconds);
void set_slow_mover(int value);
void set_nodestime(int value);
int get_move_overhead();
int get_minimum_thinking_time();
int get_slow_mover();
int get_nodestime();

void request_stop_search();

}  // namespace sirio

