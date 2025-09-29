#include "engine/eval/nnue/nnue.hpp"

#include "engine/core/board.hpp"
#include "engine/eval/nnue/evaluator.hpp"
#include "engine/uci/options.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace engine::nnue_runtime {

namespace {

std::mutex load_mutex;
std::atomic<bool> pending_reload{true};
std::atomic<bool> loaded{false};
std::string loaded_path;
engine::nnue::Evaluator evaluator;

std::mutex thread_mutex;
std::unordered_set<int> active_threads;

std::string now_utc() {
    using clock = std::chrono::system_clock;
    auto now = clock::now();
    std::time_t t = clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%SZ");
    return oss.str();
}

std::string format_size(std::uintmax_t bytes) {
    if (bytes == 0) return {};
    constexpr double kMiB = 1024.0 * 1024.0;
    double mib = static_cast<double>(bytes) / kMiB;
    std::ostringstream oss;
    if (mib >= 100.0) {
        oss << std::fixed << std::setprecision(0);
    } else if (mib >= 10.0) {
        oss << std::fixed << std::setprecision(1);
    } else {
        oss << std::fixed << std::setprecision(2);
    }
    oss << mib << "MiB";
    return oss.str();
}

std::string parse_shape(const std::string& architecture) {
    std::vector<std::string> numbers;
    std::string current;
    for (char c : architecture) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            current.push_back(c);
        } else if (!current.empty()) {
            numbers.push_back(current);
            current.clear();
        }
    }
    if (!current.empty()) {
        numbers.push_back(current);
    }
    if (numbers.empty()) return {};
    std::ostringstream oss;
    oss << '(';
    for (size_t i = 0; i < numbers.size(); ++i) {
        if (i != 0) oss << ", ";
        oss << numbers[i];
    }
    oss << ')';
    return oss.str();
}

void print_load_banner(const std::string& path) {
    const std::string architecture = evaluator.architecture();
    const std::string shape = parse_shape(architecture);
    const std::string size = format_size(evaluator.network_bytes());

    std::ostringstream headline;
    headline << "NNUE evaluation using " << path;
    if (!size.empty() || !shape.empty()) {
        headline << " (";
        bool need_comma = false;
        if (!size.empty()) {
            headline << size;
            need_comma = true;
        }
        if (!shape.empty()) {
            if (need_comma) headline << ", ";
            headline << shape;
        }
        headline << ')';
    }

    std::cout << "info string " << headline.str() << '\n';
    std::cout << "info string     source: file\n";
    std::cout << "info string     loaded: " << now_utc() << '\n';
    if (!architecture.empty()) {
        std::cout << "info string     architecture: " << architecture << '\n';
    }
    std::cout << std::flush;
}

} // namespace

bool is_loaded() { return loaded.load(std::memory_order_acquire); }

bool is_enabled() { return Options.UseNNUE && is_loaded(); }

void request_reload() { pending_reload.store(true, std::memory_order_release); }

bool load_from_file(const std::string& path, std::string* outInfo) {
    if (path.empty()) return false;

    if (!evaluator.load_network(path)) {
        return false;
    }

    loaded_path = path;
    loaded.store(true, std::memory_order_release);
    if (outInfo) {
        std::ostringstream oss;
        oss << "NNUE evaluation using " << path;
        const std::string size = format_size(evaluator.network_bytes());
        const std::string shape = parse_shape(evaluator.architecture());
        if (!size.empty() || !shape.empty()) {
            oss << " (";
            bool need_comma = false;
            if (!size.empty()) {
                oss << size;
                need_comma = true;
            }
            if (!shape.empty()) {
                if (need_comma) oss << ", ";
                oss << shape;
            }
            oss << ')';
        }
        *outInfo = oss.str();
    }
    print_load_banner(path);
    return true;
}

void try_reload_if_requested() {
    if (!pending_reload.load(std::memory_order_acquire)) {
        return;
    }

    std::lock_guard<std::mutex> lock(load_mutex);
    if (!pending_reload.load(std::memory_order_acquire)) {
        return;
    }
    pending_reload.store(false, std::memory_order_release);

    if (!Options.UseNNUE) {
        loaded.store(false, std::memory_order_release);
        loaded_path.clear();
        std::cout << "info string NNUE disabled by option\n" << std::flush;
        return;
    }

    if (!Options.EvalFile.empty()) {
        if (load_from_file(Options.EvalFile, nullptr)) {
            return;
        }
    }
    if (!Options.EvalFileSmall.empty()) {
        if (load_from_file(Options.EvalFileSmall, nullptr)) {
            return;
        }
    }

    loaded.store(false, std::memory_order_release);
    loaded_path.clear();
    std::cout << "info string NNUE disabled or file not found (EvalFile/EvalFileSmall)\n"
              << std::flush;
}

int evaluate(const Board& pos) {
    if (!is_enabled()) {
        return 0;
    }
    int score = evaluator.eval_cp(pos);
    return std::clamp(score, -30000, 30000);
}

void on_new_game() {
    // Reset any incremental state if needed in the future.
}

void on_thread_start(int tid) {
    std::lock_guard<std::mutex> lock(thread_mutex);
    active_threads.insert(tid);
    (void)tid;
}

void on_thread_stop(int tid) {
    std::lock_guard<std::mutex> lock(thread_mutex);
    active_threads.erase(tid);
    (void)tid;
}

} // namespace engine::nnue_runtime

