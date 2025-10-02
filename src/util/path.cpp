#include "util/path.h"

#include <system_error>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <unistd.h>
#endif

namespace {
namespace fs = std::filesystem;

static bool exists_file(const fs::path& p) {
  std::error_code ec;
  return fs::is_regular_file(p, ec);
}

static fs::path weakly_canonical_or_empty(const fs::path& p) {
  std::error_code ec;
  auto canonical = fs::weakly_canonical(p, ec);
  if (ec) {
    canonical = fs::absolute(p, ec);
  }
  if (ec) {
    return {};
  }
  return canonical;
}

}  // namespace

namespace util {

fs::path exe_dir() {
#ifdef _WIN32
  char buf[MAX_PATH]{};
  GetModuleFileNameA(nullptr, buf, MAX_PATH);
  return weakly_canonical_or_empty(fs::path(buf)).parent_path();
#else
  char buf[4096]{};
  ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n > 0) {
    buf[n] = '\0';
    return weakly_canonical_or_empty(fs::path(buf)).parent_path();
  }
  std::error_code ec;
  return fs::current_path(ec);
#endif
}

fs::path resolve_resource(const std::string& relOrAbs) {
  fs::path p(relOrAbs);
  if (exists_file(p)) {
    return weakly_canonical_or_empty(p);
  }

  const fs::path base = exe_dir();

  fs::path try1 = base / p;
  if (exists_file(try1)) {
    return weakly_canonical_or_empty(try1);
  }

  fs::path try2 = base / ".." / p;
  if (exists_file(try2)) {
    return weakly_canonical_or_empty(try2);
  }

  fs::path try3 = fs::current_path() / p;
  if (exists_file(try3)) {
    return weakly_canonical_or_empty(try3);
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
