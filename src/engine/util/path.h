#pragma once

#ifdef __cplusplus
#include <filesystem>
#include <string>

namespace util {
std::filesystem::path exe_dir();
std::filesystem::path resolve_resource(const std::string& relOrAbs);
}  // namespace util

extern "C" {
#endif

const char* util_exe_dir_cstr(void);
const char* util_resolve_resource_cstr(const char* rel_or_abs);

#ifdef __cplusplus
}  // extern "C"
#endif

