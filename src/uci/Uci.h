#pragma once

#include <filesystem>

extern std::filesystem::path g_engine_dir;

namespace uci {
void loop();
}

void init_options();

