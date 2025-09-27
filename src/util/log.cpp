#include "engine/util/log.hpp"
#include <iostream>

namespace engine::log {
    void info(const std::string& s)  { std::cerr << "[info] "  << s << "\n"; }
    void warn(const std::string& s)  { std::cerr << "[warn] "  << s << "\n"; }
    void error(const std::string& s) { std::cerr << "[error] " << s << "\n"; }
}
