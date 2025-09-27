#pragma once
#include <string>
namespace engine::log {
    void info(const std::string& s);
    void warn(const std::string& s);
    void error(const std::string& s);
}
