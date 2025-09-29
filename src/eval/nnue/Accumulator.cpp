#include "Accumulator.h"

namespace nnue {

void Accumulator::clear(int layerSize) {
    buf.assign(static_cast<std::size_t>(layerSize), 0);
}

} // namespace nnue
