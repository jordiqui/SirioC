#include "path.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

fs::path pathutil::exe_dir() {
#ifdef _WIN32
  wchar_t buf[MAX_PATH];
  DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  fs::path p = fs::path(buf, buf + len).parent_path();
  return p;
#else
  char buf[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (len > 0) {
    buf[len] = '\0';
    return fs::path(buf).parent_path();
  }
  return fs::current_path();
#endif
}

fs::path pathutil::first_existing(const std::vector<fs::path>& v) {
  for (const auto& p : v) {
    if (!p.empty() && fs::exists(p)) {
      return p;
    }
  }
  return {};
}

