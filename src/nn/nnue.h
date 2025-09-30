#pragma once

#include <string>

namespace nnue {
  struct State {
    bool loaded = false;
    std::string path;
    std::string dims = "(22528, 128, 8, 32, 1)";
  };

  extern State state;

  bool load(const std::string& filePath);
}

