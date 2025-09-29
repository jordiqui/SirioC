#pragma once

#include <cstdint>

namespace nnue {

struct Accumulator;
struct Move;
class Position;

void init(const Position &pos);
void update(const Move &move, Accumulator &accum);
void fill(int16_t *accumBuffer);

} // namespace nnue
