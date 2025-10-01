#include "Options.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <utility>

Options OptionsMap;

void Options::set(const std::string& name, const std::string& value) {
  auto it = find(name);
  if (it == end()) return;
  Option& o = it->second;
  switch (o.type) {
    case Option::SPIN: {
      try {
        int v = std::stoi(value);
        o.val = std::clamp(v, o.min, o.max);
      } catch (...) {
        // ignore invalid input
      }
      break;
    }
    case Option::CHECK:
      o.b = (value == "true" || value == "1");
      break;
    case Option::STRING: {
      std::string sanitized = value;
      if (sanitized == "<empty>") {
        sanitized.clear();
      } else if (sanitized.size() >= 2 && sanitized.front() == '<' &&
                 sanitized.back() == '>') {
        sanitized = sanitized.substr(1, sanitized.size() - 2);
      }
      o.s = std::move(sanitized);
      break;
    }
  }
  if (o.on_change) o.on_change();
}

