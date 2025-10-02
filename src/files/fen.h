#pragma once

#include "pyrrhic/board.h"

#include <optional>
#include <string>

namespace sirio::files {

std::optional<pyrrhic::Board> try_parse_fen(const std::string& fen,
                                            std::string* error_message = nullptr);
pyrrhic::Board parse_fen(const std::string& fen);
std::string to_fen(const pyrrhic::Board& board);

}  // namespace sirio::files
