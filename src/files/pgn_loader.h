#pragma once

#include <map>
#include <string>
#include <vector>

namespace sirio::files {

struct PgnGame {
    std::map<std::string, std::string> tags;
    std::vector<std::string> moves;
    std::string result = "*";
};

PgnGame load_pgn(const std::string& content);
PgnGame load_pgn_from_file(const std::string& path);

}  // namespace sirio::files
