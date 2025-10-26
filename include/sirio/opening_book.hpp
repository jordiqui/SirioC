#pragma once

#include <optional>
#include <string>

#include "sirio/board.hpp"
#include "sirio/move.hpp"

namespace sirio::book {

bool load(const std::string &path, std::string *error = nullptr);
void clear();
bool is_loaded();
std::optional<Move> choose_move(const Board &board);

}  // namespace sirio::book

