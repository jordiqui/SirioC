#pragma once

#include "pyrrhic/board.h"

#include <string>

namespace sirio::files {

pyrrhic::Board parse_fen(const std::string& fen);
std::string to_fen(const pyrrhic::Board& board);

}  // namespace sirio::files
