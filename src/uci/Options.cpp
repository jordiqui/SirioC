#include "Options.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

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

      const auto first = sanitized.find_first_not_of(" \t\r\n\f\v");
      if (first == std::string::npos) {
        sanitized.clear();
      } else {
        const auto last = sanitized.find_last_not_of(" \t\r\n\f\v");
        sanitized = sanitized.substr(first, last - first + 1);
      }

      o.s = std::move(sanitized);
      break;
    }
  }
  if (o.on_change) o.on_change();
}

void Options::printUci() const {
  std::vector<std::string> keys;
  keys.reserve(size());
  for (const auto& entry : *this) {
    keys.push_back(entry.first);
  }
  std::sort(keys.begin(), keys.end());

  for (const auto& key : keys) {
    const auto it = find(key);
    if (it != end()) {
      std::cout << it->second.uciDecl(key) << "\n";
    }
  }
}

