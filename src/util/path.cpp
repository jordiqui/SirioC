#include "util/path.h"

#include <system_error>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#endif

namespace {
namespace fs = std::filesystem;

fs::path canonical_or_empty(const fs::path& p) {
  std::error_code ec;
  fs::path canonical = fs::canonical(p, ec);
  if (ec) {
    canonical = fs::weakly_canonical(p, ec);
  }
  if (ec) {
    canonical = fs::absolute(p, ec);
  }
  if (ec) {
    return {};
  }
  return canonical;
}

bool exists_file(const fs::path& p) {
  std::error_code ec;
  return fs::is_regular_file(p, ec);
}

}  // namespace

namespace util {

fs::path exe_dir() {
#ifdef _WIN32
  char buf[MAX_PATH] = {0};
  DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
  if (len == 0 || len >= MAX_PATH) {
    std::error_code ec;
    return fs::current_path(ec);
  }
  fs::path exe_path(buf);
  fs::path canon = canonical_or_empty(exe_path);
  if (canon.empty()) {
    return exe_path.parent_path();
  }
  return canon.parent_path();
#else
  char buf[4096] = {0};
  ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n > 0 && n < static_cast<ssize_t>(sizeof(buf))) {
    buf[n] = '\0';
    fs::path exe_path(buf);
    fs::path canon = canonical_or_empty(exe_path);
    if (!canon.empty()) {
      return canon.parent_path();
    }
    return exe_path.parent_path();
  }
  std::error_code ec;
  return fs::current_path(ec);
#endif
}

fs::path resolve_resource(const std::string& relOrAbs) {
  if (relOrAbs.empty()) {
    return {};
  }

  fs::path candidate(relOrAbs);
  if (exists_file(candidate)) {
    return canonical_or_empty(candidate);
  }

  const fs::path base = exe_dir();
  const fs::path base_candidates[] = {
      base / relOrAbs,
      base / ".." / relOrAbs,
  };

  for (const fs::path& try_path : base_candidates) {
    if (exists_file(try_path)) {
      return canonical_or_empty(try_path);
    }
  }

  std::error_code cwd_ec;
  fs::path cwd = fs::current_path(cwd_ec);
  if (!cwd_ec) {
    fs::path cwd_candidate = cwd / relOrAbs;
    if (exists_file(cwd_candidate)) {
      return canonical_or_empty(cwd_candidate);
    }
  }

  return {};
}

}  // namespace util

extern "C" {

const char* util_exe_dir_cstr(void) {
  thread_local std::string storage;
  storage.clear();
  auto dir = util::exe_dir();
  if (dir.empty()) {
    return nullptr;
  }
  storage = dir.string();
  return storage.c_str();
}

const char* util_resolve_resource_cstr(const char* rel_or_abs) {
  if (!rel_or_abs || !*rel_or_abs) {
    return nullptr;
  }
  thread_local std::string storage;
  storage.clear();
  auto resolved = util::resolve_resource(rel_or_abs);
  if (resolved.empty()) {
    return nullptr;
  }
  storage = resolved.string();
  return storage.c_str();
}

}  // extern "C"

