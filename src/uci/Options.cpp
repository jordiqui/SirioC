#include "Options.h"

#include <algorithm>
#include <cstdlib>
#include <string>

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
    case Option::STRING:
      o.s = value;
      break;
  }
  if (o.on_change) o.on_change();
}

