#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>

struct Option {
  enum Type { SPIN, CHECK, STRING };
  Type type;
  int min = 0, max = 0, val = 0;
  bool b = false;
  std::string s;
  std::function<void()> on_change;

  std::string uciDecl(const std::string& name) const {
    switch (type) {
      case SPIN:
        return "option name " + name + " type spin default " + std::to_string(val) +
               " min " + std::to_string(min) + " max " + std::to_string(max);
      case CHECK:
        return "option name " + name + " type check default " + std::string(b ? "true" : "false");
      case STRING:
        return "option name " + name + " type string default " + (s.empty() ? "<empty>" : s);
    }
    return {};
  }
};

struct Options : public std::unordered_map<std::string, Option> {
  void set(const std::string& name, const std::string& value);
  void printUci() const {
    for (const auto& [k, v] : *this) {
      std::cout << v.uciDecl(k) << "\n";
    }
  }
};

extern Options OptionsMap;

