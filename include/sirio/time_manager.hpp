#pragma once

namespace sirio {

void set_move_overhead(int milliseconds);
void set_minimum_thinking_time(int milliseconds);
void set_slow_mover(int value);
void set_nodestime(int value);
void set_auto_time_tuning(bool enabled);
void set_moves_to_go_hint(int moves);
void record_latency_sample(int milliseconds);
void reset_time_manager_state();

int get_move_overhead();
int get_minimum_thinking_time();
int get_slow_mover();
int get_nodestime();

}  // namespace sirio

