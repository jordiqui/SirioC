#pragma once
#include <string>

namespace engine {
namespace fen {

inline constexpr const char kStartposFEN[] =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

bool is_valid_fen(const std::string& fen);

} // namespace fen
} // namespace engine
