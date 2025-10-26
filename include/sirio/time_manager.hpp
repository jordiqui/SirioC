#pragma once

namespace sirio {

void set_move_overhead(int milliseconds);
void set_minimum_thinking_time(int milliseconds);
void set_slow_mover(int value);
void set_nodestime(int value);

int get_move_overhead();
int get_minimum_thinking_time();
int get_slow_mover();
int get_nodestime();

}  // namespace sirio

