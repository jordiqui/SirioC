#pragma once

#include <cstdint>
#include <string_view>

#ifndef ENGINE_NAME
#define ENGINE_NAME "SirioC"
#endif

#ifndef ENGINE_VERSION
#define ENGINE_VERSION "0.1.0 270925"
#endif

namespace engine {
constexpr std::string_view kEngineName    = ENGINE_NAME;
constexpr std::string_view kEngineVersion = ENGINE_VERSION;
} // namespace engine
