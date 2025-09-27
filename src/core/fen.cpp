#include "engine/core/fen.hpp"
#include <sstream>
#include <vector>
#include <string>

namespace engine { namespace fen {

static inline std::vector<std::string> split(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> out; std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

bool is_valid_fen(const std::string& fen) {
    auto t = split(fen);
    // Very loose validation: 6 fields expected
    return t.size() >= 4;
}

}} // namespace
