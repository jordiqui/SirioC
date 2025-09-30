#include "nnue.h"

#include <filesystem>

namespace nnue {
  State state;

  bool load(const std::string& filePath) {
    if (filePath.empty()) {
      state.loaded = false;
      state.path.clear();
      return false;
    }
    if (!std::filesystem::exists(filePath)) {
      state.loaded = false;
      state.path.clear();
      return false;
    }
    state.loaded = true;
    state.path = filePath;
    return true;
  }
}

