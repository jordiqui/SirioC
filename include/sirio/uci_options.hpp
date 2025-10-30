#pragma once
#include <algorithm>
#include <cctype>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// Lightweight UCI options for SirioC:
// - Option types: check, spin, string, combo, button
// - Registration of essential options with sane defaults
// - Helper to print "option ..." on UCI command
// - Helper to parse "setoption ..."
//
// Header-only: you only need to include this file.
// You can attach callbacks per option via after_set().
//
// Example:
//   static sirio::uci::OptionsMap Options;
//   sirio::uci::register_essential_options(Options);
//   sirio::uci::print_uci_options(std::cout, Options);
//   // ...
//   sirio::uci::handle_setoption(Options, line); // a full "setoption ..." line

namespace sirio { namespace uci {

enum class OptionType { Check, Spin, String, Combo, Button };

struct CaseLess {
    bool operator()(const std::string& a, const std::string& b) const {
        auto f = [](unsigned char c){ return std::tolower(c); };
        size_t n = std::min(a.size(), b.size());
        for (size_t i = 0; i < n; ++i) {
            auto ca = f(a[i]), cb = f(b[i]);
            if (ca < cb) return true;
            if (ca > cb) return false;
        }
        return a.size() < b.size();
    }
};

class Option {
public:
    using OnSet = std::function<void(const Option&)>;

    // Constructors for each type
    Option() : type_(OptionType::String), i_(0), min_(0), max_(0), b_(false) {}

    // check
    explicit Option(bool v, OnSet cb = {}) : type_(OptionType::Check), i_(0), min_(0), max_(0), b_(v), s_(), cb_(std::move(cb)) {}
    // spin
    Option(int v, int minV, int maxV, OnSet cb = {}) : type_(OptionType::Spin), i_(v), min_(minV), max_(maxV), b_(false), s_(), cb_(std::move(cb)) {}
    // string
    Option(const char* v, OnSet cb = {}) : type_(OptionType::String), i_(0), min_(0), max_(0), b_(false), s_(v), cb_(std::move(cb)) {}
    Option(const std::string& v, OnSet cb = {}) : type_(OptionType::String), i_(0), min_(0), max_(0), b_(false), s_(v), cb_(std::move(cb)) {}
    // combo
    static Option Combo(const std::string& def, std::vector<std::string> vars, OnSet cb = {}) {
        Option o;
        o.type_ = OptionType::Combo;
        o.s_ = def;
        o.combo_ = std::move(vars);
        o.cb_ = std::move(cb);
        return o;
    }
    // button
    static Option Button(OnSet cb) {
        Option o;
        o.type_ = OptionType::Button;
        o.cb_ = std::move(cb);
        return o;
    }

    // after_set (callback)
    Option& after_set(OnSet cb){ cb_ = std::move(cb); return *this; }

    // Assign/value handling
    void set_bool(bool v){ b_ = v; if (cb_) cb_(*this); }
    void set_int(int v){
        if (type_ == OptionType::Spin){
            if (v < min_) v = min_;
            if (v > max_) v = max_;
            i_ = v;
            if (cb_) cb_(*this);
        }
    }
    void set_string(std::string v){
        if (type_ == OptionType::Combo){
            // must match one of combo_
            auto it = std::find_if(combo_.begin(), combo_.end(), [&](const std::string& x){
                return equal_nocase(x, v);
            });
            if (it != combo_.end()) s_ = *it;
        } else {
            s_ = std::move(v);
        }
        if (cb_) cb_(*this);
    }
    void press_button(){ if (type_ == OptionType::Button && cb_) cb_(*this); }

    // Implicit casts for convenience
    explicit operator bool() const { return b_; }
    explicit operator int()  const { return i_; }
    explicit operator std::string() const { return s_; }

    // Introspection
    OptionType type() const { return type_; }
    int  min() const { return min_; }
    int  max() const { return max_; }
    const std::vector<std::string>& vars() const { return combo_; }

    // Printing support
    void print_uci_line(std::ostream& os, const std::string& name) const {
        os << "option name " << name << " type " << type_str();
        switch (type_){
            case OptionType::Check:
                os << " default " << (b_ ? "true" : "false") << "\n";
                break;
            case OptionType::Spin:
                os << " default " << i_ << " min " << min_ << " max " << max_ << "\n";
                break;
            case OptionType::String:
                os << " default " << escape_spaces(s_) << "\n";
                break;
            case OptionType::Combo:
                os << " default " << escape_spaces(s_);
                for (auto& v : combo_) os << " var " << escape_spaces(v);
                os << "\n";
                break;
            case OptionType::Button:
                os << "\n";
                break;
        }
    }

private:
    OptionType type_;
    int i_;
    int min_, max_;
    bool b_;
    std::string s_;
    std::vector<std::string> combo_;
    OnSet cb_;

    static std::string escape_spaces(const std::string& in){
        // UCI allows spaces; we just print verbatim.
        // If you prefer quoting, adapt here.
        return in.empty() ? "<empty>" : in;
    }
    static bool equal_nocase(const std::string& a, const std::string& b){
        if (a.size() != b.size()) return false;
        for (size_t i=0;i<a.size();++i)
            if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i]))
                return false;
        return true;
    }
    const char* type_str() const {
        switch (type_){
            case OptionType::Check:  return "check";
            case OptionType::Spin:   return "spin";
            case OptionType::String: return "string";
            case OptionType::Combo:  return "combo";
            case OptionType::Button: return "button";
        }
        return "string";
    }
};

using OptionsMap = std::map<std::string, Option, CaseLess>;

// --- Essentials registration -------------------------------------------------
inline void register_essential_options(OptionsMap& o){
    // Performance / parallelism
    o["Threads"]       = Option(1, 1, 1024);
    o["Hash"]          = Option(16, 1, 33554432);  // MB
    o["Clear Hash"]    = Option::Button(nullptr);  // attach later: TT.clear()
    o["Ponder"]        = Option(false);

    // Search / analysis
    o["MultiPV"]       = Option(1, 1, 256);
    o["UCI_Chess960"]  = Option(false);
    o["UCI_ShowWDL"]   = Option(false);

    // Time management
    o["Move Overhead"]         = Option(10, 0, 5000);
    o["Minimum Thinking Time"] = Option(100, 0, 5000);
    o["Slow Mover"]            = Option(100, 10, 1000);
    o["AutoTimeTuning"]        = Option(true);

    // Strength limiting
    o["UCI_LimitStrength"] = Option(false);
    o["UCI_AnalyseMode"] = Option(false);
    o["UCI_Elo"]           = Option(1320, 1320, 3190);

    // Paths / logs / NNUE / TBs
    o["Debug Log File"]  = Option(std::string(""));
    o["EvalFile"]        = Option(std::string("nn-1c0000000000.nnue"));
    o["SyzygyPath"]      = Option(std::string(""));
    o["SyzygyProbeDepth"]= Option(1, 1, 100);
    o["Syzygy50MoveRule"]= Option(true);

    // NUMA (combo)
    o["NumaPolicy"] = Option::Combo("auto", {"auto","interleave","compact","numa0","numa1"});
}

// --- Printers ----------------------------------------------------------------
inline void print_uci_options(std::ostream& os, const OptionsMap& o){
    for (auto& kv : o)
        kv.second.print_uci_line(os, kv.first);
}

// --- Parser for "setoption name ... value ..." --------------------------------
// Accepts a full "setoption ..." line or just the tail after 'setoption '.
inline bool handle_setoption(OptionsMap& o, const std::string& full_line){
    // We accept both:
    //   setoption name X value Y Y Y
    //   name X value Y Y Y
    std::string s = full_line;
    auto lcase = [](char c){ return char(std::tolower((unsigned char)c)); };
    auto starts_with = [&](const std::string& p){
        if (s.size() < p.size()) return false;
        for (size_t i=0;i<p.size();++i) if (lcase(s[i]) != lcase(p[i])) return false;
        return true;
    };
    if (starts_with("setoption")) {
        // strip "setoption"
        size_t pos = s.find_first_of(" \t");
        if (pos != std::string::npos) s = s.substr(pos+1);
    }
    // Now parse "name ... value ..."
    auto trim = [](std::string t){
        size_t a = t.find_first_not_of(" \t\r\n");
        size_t b = t.find_last_not_of(" \t\r\n");
        if (a==std::string::npos) return std::string();
        return t.substr(a, b-a+1);
    };
    auto find_kw = [&](const std::string& kw)->size_t{
        auto pos = s.find(kw);
        if (pos == std::string::npos) {
            // case-insensitive find
            auto sl = s; std::transform(sl.begin(), sl.end(), sl.begin(), lcase);
            auto kl = kw; std::transform(kl.begin(), kl.end(), kl.begin(), lcase);
            pos = sl.find(kl);
        }
        return pos;
    };

    auto p_name = find_kw("name");
    if (p_name == std::string::npos) return false;
    p_name += 4;
    auto p_value = find_kw("value");
    std::string name = trim(p_value==std::string::npos ? s.substr(p_name) : s.substr(p_name, p_value - p_name));
    std::string value = p_value==std::string::npos ? std::string() : trim(s.substr(p_value+5));

    if (name.empty()) return false;
    auto it = o.find(name);
    if (it == o.end()) return false; // unknown

    Option& opt = it->second;
    switch (opt.type()){
        case OptionType::Check: {
            if (value.empty()) return false;
            std::string vl = value; for (auto& c: vl) c = char(std::tolower((unsigned char)c));
            bool b = (vl=="true" || vl=="1" || vl=="on" || vl=="yes");
            opt.set_bool(b);
            return true;
        }
        case OptionType::Spin: {
            if (value.empty()) return false;
            int v = 0;
            try { v = std::stoi(value); }
            catch (...) { return false; }
            opt.set_int(v);
            return true;
        }
        case OptionType::String: {
            opt.set_string(value);
            return true;
        }
        case OptionType::Combo: {
            opt.set_string(value); // validates against "var"
            return true;
        }
        case OptionType::Button: {
            opt.press_button();
            return true;
        }
    }
    return false;
}

}} // namespace sirio::uci
