#pragma once

#include <filesystem>
#include <string>

extern std::filesystem::path g_engine_dir;

namespace uci {
void loop();

// Exposed for regression tests to validate NNUE path resolution behaviour.
std::filesystem::path resolve_nnue_path_for_tests(const std::string& value);
}

void init_options();

