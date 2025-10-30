#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

#include "sirio/board.hpp"
#include "sirio/evaluation.hpp"
#include "sirio/move.hpp"
#include "sirio/movegen.hpp"
#include "sirio/nnue/api.hpp"
#include "sirio/opening_book.hpp"
#include "sirio/search.hpp"
#include "sirio/syzygy.hpp"
#include "sirio/time_manager.hpp"
#include "sirio/transposition_table.hpp"
#include "sirio/uci.hpp"
#include "sirio/uci_options.hpp"

namespace sirio {
namespace uci {
namespace {

constexpr char kDefaultEvalFile[] = "nn-1c0000000000.nnue";
constexpr char kDefaultEvalFileSmall[] = "nn-37f18f62d772.nnue";

std::string pending_eval_file{kDefaultEvalFile};
std::string pending_eval_file_small{kDefaultEvalFileSmall};
std::ofstream debug_log_stream;
std::streambuf* original_clog_buffer = nullptr;

std::mutex search_thread_mutex;
std::thread search_thread;
std::atomic<bool> search_in_progress{false};

std::mutex pending_result_mutex;
std::optional<sirio::SearchResult> pending_infinite_result;

struct EngineOptions {
    std::string debug_log_file;
    std::string numa_policy = "auto";
    int threads = 1;
    std::size_t hash_size_mb = 16;
    bool ponder = false;
    int multi_pv = 1;
    int analysis_lines = 1;
    int skill_level = 20;
    int move_overhead = 10;
    int minimum_thinking_time = 100;
    int slow_mover = 100;
    int nodestime = 0;
    bool auto_time_tuning = true;
    bool uci_chess960 = false;
    bool uci_limit_strength = false;
    bool uci_analyse_mode = false;
    int uci_elo = 1320;
    bool uci_show_wdl = false;
    std::string syzygy_path;
    int syzygy_probe_depth = 1;
    bool syzygy_50_move_rule = true;
    int syzygy_probe_limit = 7;
    bool hash_persist = false;
    std::string hash_persist_file;
    bool use_book = false;
    std::string book_file;
    bool persistent_analysis = false;
    std::string persistent_analysis_file;
};

EngineOptions options;
bool persistent_analysis_loaded = false;

OptionsMap g_options;
bool g_options_registered = false;
bool g_silent_option_update = false;
sirio::Board* g_option_board = nullptr;

struct SilentUpdateGuard {
    SilentUpdateGuard() : previous(g_silent_option_update) { g_silent_option_update = true; }
    ~SilentUpdateGuard() { g_silent_option_update = previous; }

    bool previous;
};

bool load_transposition_table_with_feedback(const std::string& path, std::string_view success_name,
                                            std::string_view failure_name, bool verbose_success) {
    std::string error;
    if (sirio::load_transposition_table(path, &error)) {
        if (verbose_success) {
            std::cout << "info string " << success_name << " loaded from " << path << std::endl;
        }
        return true;
    }
    std::cout << "info string Failed to load " << failure_name << ": " << error << std::endl;
    return false;
}

bool save_transposition_table_with_feedback(const std::string& path, std::string_view success_name,
                                            std::string_view failure_name, bool verbose_success) {
    std::string error;
    if (sirio::save_transposition_table(path, &error)) {
        if (verbose_success) {
            std::cout << "info string " << success_name << " saved to " << path << std::endl;
        }
        return true;
    }
    std::cout << "info string Failed to save " << failure_name << ": " << error << std::endl;
    return false;
}

void mark_persistent_analysis_unloaded() { persistent_analysis_loaded = false; }

void try_autoload_persistent_analysis(bool verbose_success) {
    if (!options.persistent_analysis) {
        return;
    }
    if (options.persistent_analysis_file.empty()) {
        return;
    }
    if (persistent_analysis_loaded) {
        return;
    }
    if (load_transposition_table_with_feedback(options.persistent_analysis_file,
                                               "Persistent analysis table", "persistent analysis table",
                                               verbose_success)) {
        persistent_analysis_loaded = true;
    }
}

void save_persistent_analysis_if_enabled(bool verbose_success) {
    if (!options.persistent_analysis || options.persistent_analysis_file.empty()) {
        return;
    }
    if (save_transposition_table_with_feedback(options.persistent_analysis_file,
                                               "Persistent analysis table", "persistent analysis table",
                                               verbose_success)) {
        persistent_analysis_loaded = true;
    }
}

void apply_time_management_options() {
    sirio::set_move_overhead(options.move_overhead);
    sirio::set_minimum_thinking_time(options.minimum_thinking_time);
    sirio::set_slow_mover(options.slow_mover);
    sirio::set_nodestime(options.nodestime);
    sirio::set_auto_time_tuning(options.auto_time_tuning);
}

void initialize_engine_options() {
    if (original_clog_buffer == nullptr) {
        original_clog_buffer = std::clog.rdbuf();
    }
    options.threads = sirio::recommended_search_threads();
    sirio::set_search_threads(options.threads);
    sirio::set_transposition_table_size(options.hash_size_mb);
    mark_persistent_analysis_unloaded();
    apply_time_management_options();
    if (options.syzygy_path.empty()) {
        if (auto detected = sirio::syzygy::detect_default_tablebase_path(); detected.has_value()) {
            options.syzygy_path = detected->string();
        }
    }
    if (!options.syzygy_path.empty()) {
        sirio::syzygy::set_tablebase_path(options.syzygy_path);
    }
    sirio::syzygy::set_probe_depth_limit(options.syzygy_probe_depth);
    sirio::syzygy::set_probe_piece_limit(options.syzygy_probe_limit);
    sirio::syzygy::set_use_fifty_move_rule(options.syzygy_50_move_rule);
}

void stop_and_join_search() {
    if (search_in_progress.load(std::memory_order_acquire)) {
        sirio::request_stop_search();
    }

    std::thread local_thread;
    {
        std::lock_guard<std::mutex> lock(search_thread_mutex);
        if (search_thread.joinable()) {
            local_thread = std::move(search_thread);
        }
    }

    if (local_thread.joinable()) {
        local_thread.join();
    }

    search_in_progress.store(false, std::memory_order_release);

    std::optional<sirio::SearchResult> pending;
    {
        std::lock_guard<std::mutex> lock(pending_result_mutex);
        if (pending_infinite_result.has_value()) {
            pending = pending_infinite_result;
            pending_infinite_result.reset();
        }
    }

    if (pending.has_value()) {
        const sirio::SearchResult& result = *pending;
        if (result.has_move) {
            std::cout << "bestmove " << sirio::move_to_uci(result.best_move);
            if (result.principal_variation.size() >= 2) {
                std::cout << " ponder "
                          << sirio::move_to_uci(result.principal_variation[1]);
            }
            std::cout << std::endl;
        } else {
            std::cout << "bestmove 0000" << std::endl;
        }
    }
}

void output_search_result(const sirio::Board& board, const sirio::SearchLimits& limits,
                          const sirio::SearchResult& result) {
    if (result.has_move) {
        int reported_depth = result.depth_reached > 0 ? result.depth_reached : limits.max_depth;
        if (reported_depth < 0) {
            reported_depth = 0;
        }
        int seldepth = result.seldepth > 0 ? result.seldepth : reported_depth;
        std::uint64_t nodes = result.nodes;
        int time_ms = result.time_ms;
        if (time_ms < 0) {
            time_ms = 0;
        }
        std::uint64_t nps = result.nodes_per_second > 0
                                 ? result.nodes_per_second
                                 : (time_ms > 0 ? (nodes * 1000ULL) / static_cast<std::uint64_t>(time_ms)
                                                : 0ULL);
        std::uint64_t knps_before = result.knps_before > 0 ? result.knps_before : nps / 1000ULL;
        std::uint64_t knps_after = result.knps_after > 0 ? result.knps_after : nps / 1000ULL;
        std::string pv_string = sirio::principal_variation_to_uci(board, result.principal_variation);
        if (pv_string.empty()) {
            pv_string = sirio::move_to_uci(result.best_move);
        }
        std::cout << "info depth " << reported_depth << " seldepth " << seldepth
                  << " multipv 1 score " << sirio::format_uci_score(result.score)
                  << " nodes " << nodes << " nps " << nps << " knps_before " << knps_before
                  << " knps_after " << knps_after << " hashfull 0 tbhits 0 time " << time_ms
                  << " pv " << pv_string << std::endl;
        if (limits.infinite) {
            std::lock_guard<std::mutex> lock(pending_result_mutex);
            pending_infinite_result = result;
        } else {
            std::cout << "bestmove " << sirio::move_to_uci(result.best_move);
            if (result.principal_variation.size() >= 2) {
                std::cout << " ponder "
                          << sirio::move_to_uci(result.principal_variation[1]);
            }
            std::cout << std::endl;
        }
    } else {
        auto legal_moves = sirio::generate_legal_moves(board);
        if (limits.infinite) {
            sirio::SearchResult pending;
            pending.has_move = false;
            if (!legal_moves.empty()) {
                pending.best_move = legal_moves.front();
                pending.has_move = true;
            }
            std::lock_guard<std::mutex> lock(pending_result_mutex);
            pending_infinite_result = pending;
        } else {
            if (!legal_moves.empty()) {
                const auto fallback = legal_moves.front();
                std::cout << "bestmove " << sirio::move_to_uci(fallback) << std::endl;
            } else {
                std::cout << "bestmove 0000" << std::endl;
            }
        }
    }

    if (options.hash_persist && !options.hash_persist_file.empty()) {
        save_transposition_table_with_feedback(options.hash_persist_file, "Transposition table",
                                               "transposition table", false);
    }
    save_persistent_analysis_if_enabled(false);
}

void start_search_async(const sirio::Board& board, const sirio::SearchLimits& limits) {
    stop_and_join_search();

    sirio::Board board_copy = board;
    sirio::SearchLimits limits_copy = limits;

    {
        std::lock_guard<std::mutex> lock(pending_result_mutex);
        pending_infinite_result.reset();
    }

    search_in_progress.store(true, std::memory_order_release);

    std::thread new_thread;

    try {
        new_thread = std::thread([board_copy, limits_copy]() mutable {
            sirio::SearchResult result = sirio::search_best_move(board_copy, limits_copy);
            output_search_result(board_copy, limits_copy, result);
            search_in_progress.store(false, std::memory_order_release);
        });

        std::lock_guard<std::mutex> lock(search_thread_mutex);
        search_thread = std::move(new_thread);
    } catch (...) {
        if (new_thread.joinable()) {
            new_thread.join();
        }
        search_in_progress.store(false, std::memory_order_release);
        throw;
    }
}

std::string normalize_eval_path(const std::string& value) {
    if (value == "<empty>") {
        return std::string{};
    }
    return value;
}

std::string normalize_string_option(const std::string& value) {
    if (value == "<empty>") {
        return std::string{};
    }
    return value;
}

void print_loaded_nnue_info(const sirio::nnue::NetworkInfo& info) {
    std::cout << "info string NNUE evaluation using " << info.path;
    if (info.bytes != 0) {
        std::cout << " (" << (info.bytes / 1024 / 1024) << "MiB";
        if (!info.dims.empty()) {
            std::cout << ", " << info.dims;
        }
        std::cout << ")";
    } else if (!info.dims.empty()) {
        std::cout << " (" << info.dims << ")";
    }
    std::cout << std::endl;
}

bool nnue_try_load(const std::string& path, sirio::Board& board) {
    if (path.empty()) {
        return false;
    }

    std::string error;
    if (!sirio::nnue::init(path, &error)) {
        std::cout << "info string Failed to load NNUE: " << error << std::endl;
        return false;
    }

    sirio::initialize_evaluation(board);
    if (auto meta = sirio::nnue::info()) {
        print_loaded_nnue_info(*meta);
    } else {
        std::cout << "info string NNUE loaded from " << path << std::endl;
    }
    return true;
}

void nnue_load_if_pending(sirio::Board& board) {
    if (sirio::nnue::is_loaded()) {
        return;
    }
    if (!pending_eval_file.empty()) {
        if (nnue_try_load(pending_eval_file, board)) {
            return;
        }
    }
    if (!pending_eval_file_small.empty()) {
        nnue_try_load(pending_eval_file_small, board);
    }
}

Option* find_option(const std::string& name) {
    auto it = g_options.find(name);
    if (it == g_options.end()) {
        return nullptr;
    }
    return &it->second;
}

void on_debug_log_file(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    std::string path = normalize_string_option(static_cast<std::string>(opt));
    if (debug_log_stream.is_open()) {
        debug_log_stream.close();
    }
    if (!path.empty()) {
        debug_log_stream.open(path, std::ios::out | std::ios::app);
        if (debug_log_stream) {
            options.debug_log_file = path;
            if (original_clog_buffer == nullptr) {
                original_clog_buffer = std::clog.rdbuf();
            }
            std::clog.rdbuf(debug_log_stream.rdbuf());
            std::cout << "info string Debug log file set to " << path << std::endl;
        } else {
            options.debug_log_file.clear();
            if (original_clog_buffer != nullptr) {
                std::clog.rdbuf(original_clog_buffer);
            }
            std::cout << "info string Failed to open debug log file: " << path << std::endl;
        }
    } else {
        options.debug_log_file.clear();
        if (original_clog_buffer != nullptr) {
            std::clog.rdbuf(original_clog_buffer);
        }
    }
}

void on_numa_policy(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    options.numa_policy = static_cast<std::string>(opt);
}

void on_threads(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    options.threads = static_cast<int>(opt);
    sirio::set_search_threads(options.threads);
}

void on_hash(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    std::size_t clamped = static_cast<std::size_t>(static_cast<int>(opt));
    options.hash_size_mb = clamped;
    sirio::set_transposition_table_size(options.hash_size_mb);
    mark_persistent_analysis_unloaded();
}

void on_hash_file(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    std::string path = normalize_string_option(static_cast<std::string>(opt));
    options.hash_persist_file = path;
    if (path.empty()) {
        std::cout << "info string Hash persistence file cleared" << std::endl;
    } else {
        load_transposition_table_with_feedback(path, "Transposition table", "transposition table", true);
    }
}

void on_hash_persist(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    options.hash_persist = static_cast<bool>(opt);
    if (options.hash_persist && !options.hash_persist_file.empty()) {
        if (load_transposition_table_with_feedback(options.hash_persist_file, "Transposition table",
                                                   "transposition table", true)) {
            std::cout << "info string Transposition table ready from " << options.hash_persist_file
                      << std::endl;
        }
    }
}

void on_persistent_analysis(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    bool enabled = static_cast<bool>(opt);
    if (enabled != options.persistent_analysis) {
        options.persistent_analysis = enabled;
        mark_persistent_analysis_unloaded();
        if (enabled) {
            std::cout << "info string Persistent analysis mode enabled" << std::endl;
            try_autoload_persistent_analysis(true);
        } else {
            std::cout << "info string Persistent analysis mode disabled" << std::endl;
        }
    } else if (enabled) {
        try_autoload_persistent_analysis(false);
    }
}

void on_persistent_analysis_file(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    std::string path = normalize_string_option(static_cast<std::string>(opt));
    if (path == options.persistent_analysis_file) {
        if (options.persistent_analysis) {
            try_autoload_persistent_analysis(false);
        }
        return;
    }
    options.persistent_analysis_file = path;
    mark_persistent_analysis_unloaded();
    if (path.empty()) {
        std::cout << "info string Persistent analysis file cleared" << std::endl;
    } else {
        std::cout << "info string Persistent analysis file set to " << path << std::endl;
        if (options.persistent_analysis) {
            try_autoload_persistent_analysis(true);
        }
    }
}

void on_persistent_analysis_load(const Option&) {
    if (options.persistent_analysis_file.empty()) {
        std::cout << "info string Persistent analysis file not set" << std::endl;
        return;
    }
    if (load_transposition_table_with_feedback(options.persistent_analysis_file,
                                               "Persistent analysis table", "persistent analysis table", true)) {
        persistent_analysis_loaded = true;
    }
}

void on_persistent_analysis_save(const Option&) {
    if (options.persistent_analysis_file.empty()) {
        std::cout << "info string Persistent analysis file not set" << std::endl;
        return;
    }
    if (save_transposition_table_with_feedback(options.persistent_analysis_file,
                                               "Persistent analysis table", "persistent analysis table", true)) {
        persistent_analysis_loaded = true;
    }
}

void on_persistent_analysis_clear(const Option&) {
    sirio::clear_transposition_tables();
    mark_persistent_analysis_unloaded();
    std::cout << "info string Persistent analysis table cleared" << std::endl;
    if (!options.persistent_analysis_file.empty()) {
        std::error_code ec;
        if (std::filesystem::remove(options.persistent_analysis_file, ec)) {
            std::cout << "info string Persistent analysis file removed" << std::endl;
        } else if (ec) {
            std::cout << "info string Failed to remove persistent analysis file: " << ec.message()
                      << std::endl;
        }
    }
}

void on_clear_hash(const Option&) {
    sirio::clear_transposition_tables();
    mark_persistent_analysis_unloaded();
    std::cout << "info string Transposition table cleared" << std::endl;
}

void on_ponder(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    options.ponder = static_cast<bool>(opt);
}

void on_multipv(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    int value = static_cast<int>(opt);
    options.multi_pv = value;
    options.analysis_lines = value;
    if (auto* other = find_option("Analysis Lines")) {
        SilentUpdateGuard guard;
        other->set_int(value);
    }
}

void on_analysis_lines(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    int value = static_cast<int>(opt);
    options.analysis_lines = value;
    options.multi_pv = value;
    if (auto* other = find_option("MultiPV")) {
        SilentUpdateGuard guard;
        other->set_int(value);
    }
}

void on_skill_level(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    options.skill_level = static_cast<int>(opt);
}

void on_move_overhead(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    options.move_overhead = static_cast<int>(opt);
    apply_time_management_options();
}

void on_minimum_thinking_time(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    options.minimum_thinking_time = static_cast<int>(opt);
    apply_time_management_options();
}

void on_slow_mover(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    options.slow_mover = static_cast<int>(opt);
    apply_time_management_options();
}

void on_nodes_time(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    options.nodestime = static_cast<int>(opt);
    apply_time_management_options();
}

void on_auto_time_tuning(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    options.auto_time_tuning = static_cast<bool>(opt);
    apply_time_management_options();
}

void on_uci_chess960(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    options.uci_chess960 = static_cast<bool>(opt);
}

void on_uci_limit_strength(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    options.uci_limit_strength = static_cast<bool>(opt);
}

void on_uci_analyse_mode(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    options.uci_analyse_mode = static_cast<bool>(opt);
}

void on_uci_elo(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    options.uci_elo = static_cast<int>(opt);
}

void on_uci_show_wdl(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    options.uci_show_wdl = static_cast<bool>(opt);
}

void on_syzygy_path(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    std::string path = normalize_string_option(static_cast<std::string>(opt));
    options.syzygy_path = path;
    sirio::syzygy::set_tablebase_path(path);
}

void on_syzygy_probe_depth(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    options.syzygy_probe_depth = static_cast<int>(opt);
    sirio::syzygy::set_probe_depth_limit(options.syzygy_probe_depth);
}

void on_syzygy_50_move_rule(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    options.syzygy_50_move_rule = static_cast<bool>(opt);
    sirio::syzygy::set_use_fifty_move_rule(options.syzygy_50_move_rule);
}

void on_syzygy_probe_limit(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    options.syzygy_probe_limit = static_cast<int>(opt);
    sirio::syzygy::set_probe_piece_limit(options.syzygy_probe_limit);
}

void on_eval_file(const Option& opt) {
    std::string raw = static_cast<std::string>(opt);
    std::string normalized = normalize_eval_path(raw);
    pending_eval_file = normalized;
    if (g_silent_option_update) {
        return;
    }
    if (normalized.empty()) {
        sirio::nnue::unload();
        if (g_option_board != nullptr) {
            sirio::initialize_evaluation(*g_option_board);
        }
        std::cout << "info string NNUE evaluation disabled" << std::endl;
        return;
    }
    if (g_option_board != nullptr) {
        nnue_try_load(normalized, *g_option_board);
    }
}

void on_eval_file_small(const Option& opt) {
    std::string raw = static_cast<std::string>(opt);
    std::string normalized = normalize_eval_path(raw);
    pending_eval_file_small = normalized;
    if (g_silent_option_update) {
        return;
    }
    if (g_option_board != nullptr) {
        nnue_try_load(normalized, *g_option_board);
    }
}

void on_use_book(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    options.use_book = static_cast<bool>(opt);
    if (options.use_book && !options.book_file.empty() && !sirio::book::is_loaded()) {
        std::string error;
        if (sirio::book::load(options.book_file, &error)) {
            std::cout << "info string Opening book loaded from " << options.book_file << std::endl;
        } else {
            std::cout << "info string Failed to load opening book: " << error << std::endl;
            options.book_file.clear();
        }
    }
}

void on_book_file(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    std::string path = normalize_string_option(static_cast<std::string>(opt));
    options.book_file = path;
    if (path.empty()) {
        sirio::book::clear();
        std::cout << "info string Opening book cleared" << std::endl;
    } else {
        std::string error;
        if (sirio::book::load(path, &error)) {
            std::cout << "info string Opening book loaded from " << path << std::endl;
        } else {
            std::cout << "info string Failed to load opening book: " << error << std::endl;
            options.book_file.clear();
        }
    }
}

void on_use_nnue(const Option& opt) {
    if (g_silent_option_update) {
        return;
    }
    bool enabled = static_cast<bool>(opt);
    if (!enabled) {
        pending_eval_file.clear();
        pending_eval_file_small.clear();
        sirio::nnue::unload();
        if (g_option_board != nullptr) {
            sirio::initialize_evaluation(*g_option_board);
        }
        std::cout << "info string NNUE evaluation disabled" << std::endl;
    }
}

void on_nnue_file(const Option& opt) {
    std::string raw = static_cast<std::string>(opt);
    std::string normalized = normalize_eval_path(raw);
    pending_eval_file = normalized;
    if (g_silent_option_update) {
        return;
    }
    if (normalized.empty()) {
        sirio::nnue::unload();
        if (g_option_board != nullptr) {
            sirio::initialize_evaluation(*g_option_board);
        }
        std::cout << "info string NNUE evaluation disabled" << std::endl;
    } else if (g_option_board != nullptr) {
        nnue_try_load(normalized, *g_option_board);
    }
}

void ensure_options_registered() {
    if (g_options_registered) {
        return;
    }

    register_essential_options(g_options);

    g_options["Debug Log File"].after_set(on_debug_log_file);
    g_options["NumaPolicy"].after_set(on_numa_policy);
    g_options["Threads"].after_set(on_threads);
    g_options["Hash"].after_set(on_hash);
    g_options["Clear Hash"].after_set(on_clear_hash);
    g_options["Ponder"].after_set(on_ponder);
    g_options["MultiPV"].after_set(on_multipv);
    g_options["UCI_Chess960"].after_set(on_uci_chess960);
    g_options["UCI_ShowWDL"].after_set(on_uci_show_wdl);
    g_options["Move Overhead"].after_set(on_move_overhead);
    g_options["Minimum Thinking Time"].after_set(on_minimum_thinking_time);
    g_options["Slow Mover"].after_set(on_slow_mover);
    g_options["AutoTimeTuning"].after_set(on_auto_time_tuning);
    g_options["UCI_LimitStrength"].after_set(on_uci_limit_strength);
    g_options["UCI_AnalyseMode"].after_set(on_uci_analyse_mode);
    g_options["UCI_Elo"].after_set(on_uci_elo);
    g_options["EvalFile"].after_set(on_eval_file);
    g_options["SyzygyPath"].after_set(on_syzygy_path);
    g_options["SyzygyProbeDepth"].after_set(on_syzygy_probe_depth);
    g_options["Syzygy50MoveRule"].after_set(on_syzygy_50_move_rule);

    g_options["HashFile"] = Option(std::string(""));
    g_options["HashFile"].after_set(on_hash_file);

    g_options["HashPersist"] = Option(false);
    g_options["HashPersist"].after_set(on_hash_persist);

    g_options["PersistentAnalysis"] = Option(false);
    g_options["PersistentAnalysis"].after_set(on_persistent_analysis);

    g_options["PersistentAnalysisFile"] = Option(std::string(""));
    g_options["PersistentAnalysisFile"].after_set(on_persistent_analysis_file);

    g_options["PersistentAnalysisLoad"] = Option::Button(on_persistent_analysis_load);
    g_options["PersistentAnalysisSave"] = Option::Button(on_persistent_analysis_save);
    g_options["PersistentAnalysisClear"] = Option::Button(on_persistent_analysis_clear);

    g_options["Analysis Lines"] = Option(1, 1, 256);
    g_options["Analysis Lines"].after_set(on_analysis_lines);

    g_options["Skill Level"] = Option(20, 0, 20);
    g_options["Skill Level"].after_set(on_skill_level);

    g_options["nodestime"] = Option(0, 0, 10000);
    g_options["nodestime"].after_set(on_nodes_time);

    g_options["AutoTimeTuning"] = Option(true);
    g_options["AutoTimeTuning"].after_set(on_auto_time_tuning);

    g_options["SyzygyProbeLimit"] = Option(7, 0, 7);
    g_options["SyzygyProbeLimit"].after_set(on_syzygy_probe_limit);

    g_options["EvalFileSmall"] = Option(std::string(kDefaultEvalFileSmall));
    g_options["EvalFileSmall"].after_set(on_eval_file_small);

    g_options["UseBook"] = Option(false);
    g_options["UseBook"].after_set(on_use_book);

    g_options["BookFile"] = Option(std::string(""));
    g_options["BookFile"].after_set(on_book_file);

    g_options["UseNNUE"] = Option(true);
    g_options["UseNNUE"].after_set(on_use_nnue);

    g_options["NNUEFile"] = Option(std::string(kDefaultEvalFile));
    g_options["NNUEFile"].after_set(on_nnue_file);

    g_options_registered = true;
}

void sync_options_from_state() {
    if (!g_options_registered) {
        return;
    }

    SilentUpdateGuard guard;
    if (auto* opt = find_option("Debug Log File")) {
        opt->set_string(options.debug_log_file);
    }
    if (auto* opt = find_option("NumaPolicy")) {
        opt->set_string(options.numa_policy);
    }
    if (auto* opt = find_option("Threads")) {
        opt->set_int(options.threads);
    }
    if (auto* opt = find_option("Hash")) {
        opt->set_int(static_cast<int>(options.hash_size_mb));
    }
    if (auto* opt = find_option("Ponder")) {
        opt->set_bool(options.ponder);
    }
    if (auto* opt = find_option("MultiPV")) {
        opt->set_int(options.multi_pv);
    }
    if (auto* opt = find_option("Analysis Lines")) {
        opt->set_int(options.analysis_lines);
    }
    if (auto* opt = find_option("Skill Level")) {
        opt->set_int(options.skill_level);
    }
    if (auto* opt = find_option("Move Overhead")) {
        opt->set_int(options.move_overhead);
    }
    if (auto* opt = find_option("Minimum Thinking Time")) {
        opt->set_int(options.minimum_thinking_time);
    }
    if (auto* opt = find_option("Slow Mover")) {
        opt->set_int(options.slow_mover);
    }
    if (auto* opt = find_option("AutoTimeTuning")) {
        opt->set_bool(options.auto_time_tuning);
    }
    if (auto* opt = find_option("nodestime")) {
        opt->set_int(options.nodestime);
    }
    if (auto* opt = find_option("AutoTimeTuning")) {
        opt->set_bool(options.auto_time_tuning);
    }
    if (auto* opt = find_option("UCI_Chess960")) {
        opt->set_bool(options.uci_chess960);
    }
    if (auto* opt = find_option("UCI_LimitStrength")) {
        opt->set_bool(options.uci_limit_strength);
    }
    if (auto* opt = find_option("UCI_AnalyseMode")) {
        opt->set_bool(options.uci_analyse_mode);
    }
    if (auto* opt = find_option("UCI_Elo")) {
        opt->set_int(options.uci_elo);
    }
    if (auto* opt = find_option("UCI_ShowWDL")) {
        opt->set_bool(options.uci_show_wdl);
    }
    if (auto* opt = find_option("SyzygyPath")) {
        opt->set_string(options.syzygy_path);
    }
    if (auto* opt = find_option("SyzygyProbeDepth")) {
        opt->set_int(options.syzygy_probe_depth);
    }
    if (auto* opt = find_option("Syzygy50MoveRule")) {
        opt->set_bool(options.syzygy_50_move_rule);
    }
    if (auto* opt = find_option("SyzygyProbeLimit")) {
        opt->set_int(options.syzygy_probe_limit);
    }
    if (auto* opt = find_option("HashFile")) {
        opt->set_string(options.hash_persist_file);
    }
    if (auto* opt = find_option("HashPersist")) {
        opt->set_bool(options.hash_persist);
    }
    if (auto* opt = find_option("PersistentAnalysis")) {
        opt->set_bool(options.persistent_analysis);
    }
    if (auto* opt = find_option("PersistentAnalysisFile")) {
        opt->set_string(options.persistent_analysis_file);
    }
    if (auto* opt = find_option("EvalFile")) {
        opt->set_string(pending_eval_file);
    }
    if (auto* opt = find_option("EvalFileSmall")) {
        opt->set_string(pending_eval_file_small);
    }
    if (auto* opt = find_option("UseBook")) {
        opt->set_bool(options.use_book);
    }
    if (auto* opt = find_option("BookFile")) {
        opt->set_string(options.book_file);
    }
    if (auto* opt = find_option("UseNNUE")) {
        bool enabled = !pending_eval_file.empty() || !pending_eval_file_small.empty() || sirio::nnue::is_loaded();
        opt->set_bool(enabled);
    }
    if (auto* opt = find_option("NNUEFile")) {
        opt->set_string(pending_eval_file);
    }
}

bool handle_setoption(const std::string& args, sirio::Board& board) {
    ensure_options_registered();
    g_option_board = &board;
    bool handled = sirio::uci::handle_setoption(g_options, args);
    g_option_board = nullptr;
    return handled;
}

void send_uci_id() {
    ensure_options_registered();
    sync_options_from_state();
    std::cout << "id name SirioC" << std::endl;
    std::cout << "id author Jorge Ruiz Centelles" << std::endl;
    print_uci_options(std::cout, g_options);
    std::cout << "uciok" << std::endl;
}

void send_ready(sirio::Board& board) {
    nnue_load_if_pending(board);
    std::cout << "readyok" << std::endl;
}

std::string trim_leading(std::string_view view) {
    std::size_t pos = 0;
    while (pos < view.size() && std::isspace(static_cast<unsigned char>(view[pos]))) {
        ++pos;
    }
    return std::string{view.substr(pos)};
}

std::string trim_whitespace(std::string_view view) {
    std::size_t start = 0;
    while (start < view.size() && std::isspace(static_cast<unsigned char>(view[start]))) {
        ++start;
    }
    std::size_t end = view.size();
    while (end > start && std::isspace(static_cast<unsigned char>(view[end - 1]))) {
        --end;
    }
    return std::string{view.substr(start, end - start)};
}

void set_position(sirio::Board& board, const std::string& command_args) {
    std::istringstream stream{command_args};
    std::string token;
    if (!(stream >> token)) {
        return;
    }

    if (token == "startpos") {
        board = sirio::Board{};
        if (!(stream >> token)) {
            sirio::initialize_evaluation(board);
            return;
        }
    } else if (token == "fen") {
        std::string fen_parts[6];
        for (int i = 0; i < 6; ++i) {
            if (!(stream >> fen_parts[i])) {
                throw std::runtime_error("Invalid FEN in position command");
            }
        }
        board = sirio::Board{fen_parts[0] + " " + fen_parts[1] + " " + fen_parts[2] + " " +
                             fen_parts[3] + " " + fen_parts[4] + " " + fen_parts[5]};
        if (!(stream >> token)) {
            sirio::initialize_evaluation(board);
            return;
        }
    } else {
        throw std::runtime_error("Unsupported position command");
    }

    if (token != "moves") {
        while (stream >> token) {
            if (token == "moves") {
                break;
            }
        }
    }

    if (token != "moves") {
        sirio::initialize_evaluation(board);
        return;
    }

    while (stream >> token) {
        if (!sirio::apply_uci_move(board, token)) {
            break;
        }
    }

    sirio::initialize_evaluation(board);
}

constexpr int kDefaultGoDepth = 128;

void handle_go(const std::string& command_args, const sirio::Board& board) {
    std::istringstream stream{command_args};
    std::string token;
    sirio::SearchLimits limits;
    bool depth_overridden = false;
    bool has_time_information = false;
    bool infinite_requested = false;
    bool any_limit_specified = false;

    try_autoload_persistent_analysis(false);

    while (stream >> token) {
        if (token == "depth") {
            if (stream >> token) {
                limits.max_depth = std::stoi(token);
                depth_overridden = true;
                if (limits.max_depth > 0) {
                    any_limit_specified = true;
                }
            }
        } else if (token == "nodes") {
            if (stream >> token) {
                long long parsed = std::stoll(token);
                if (parsed > 0) {
                    limits.max_nodes = static_cast<std::uint64_t>(parsed);
                    any_limit_specified = true;
                } else {
                    limits.max_nodes = 0;
                }
            }
        } else if (token == "movetime") {
            if (stream >> token) {
                limits.move_time = std::stoi(token);
                has_time_information = true;
                limits.max_depth = 128;
                if (limits.move_time > 0) {
                    any_limit_specified = true;
                }
            }
        } else if (token == "wtime") {
            if (stream >> token) {
                limits.time_left_white = std::stoi(token);
                has_time_information = true;
                if (limits.time_left_white > 0) {
                    any_limit_specified = true;
                }
            }
        } else if (token == "btime") {
            if (stream >> token) {
                limits.time_left_black = std::stoi(token);
                has_time_information = true;
                if (limits.time_left_black > 0) {
                    any_limit_specified = true;
                }
            }
        } else if (token == "winc") {
            if (stream >> token) {
                limits.increment_white = std::stoi(token);
                has_time_information = true;
                if (limits.increment_white > 0) {
                    any_limit_specified = true;
                }
            }
        } else if (token == "binc") {
            if (stream >> token) {
                limits.increment_black = std::stoi(token);
                has_time_information = true;
                if (limits.increment_black > 0) {
                    any_limit_specified = true;
                }
            }
        } else if (token == "movestogo") {
            if (stream >> token) {
                limits.moves_to_go = std::stoi(token);
                has_time_information = true;
                if (limits.moves_to_go > 0) {
                    any_limit_specified = true;
                }
            }
        } else if (token == "infinite") {
            infinite_requested = true;
        }
    }

    if (infinite_requested || (!any_limit_specified && !has_time_information && !depth_overridden &&
                               limits.max_nodes == 0)) {
        limits.infinite = true;
        limits.max_depth = 0;
        limits.max_nodes = 0;
        limits.move_time = 0;
        limits.time_left_white = 0;
        limits.time_left_black = 0;
        limits.increment_white = 0;
        limits.increment_black = 0;
        limits.moves_to_go = 0;
    }

    if (!limits.infinite) {
        if (has_time_information && !depth_overridden && limits.move_time == 0) {
            limits.max_depth = 128;
        }

        if (!depth_overridden && !has_time_information && limits.max_nodes == 0 &&
            any_limit_specified) {
            limits.max_depth = kDefaultGoDepth;
        }
    }

    sirio::initialize_evaluation(board);
    if (options.use_book && !options.book_file.empty() && sirio::book::is_loaded()) {
        if (auto book_move = sirio::book::choose_move(board); book_move.has_value()) {
            std::string uci = sirio::move_to_uci(*book_move);
            std::cout << "info string book move " << uci << std::endl;
            std::cout << "bestmove " << uci << std::endl;
            return;
        }
    }

    start_search_async(board, limits);
}

void handle_bench() {
    auto log = [](const std::string& message) {
        std::cout << "info string " << message << std::endl;
    };

    std::vector<std::string> speed_positions = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r1bq1rk1/ppp2ppp/2n2n2/3pp3/3P4/2P1PN2/PP1NBPPP/R2QKB1R w KQ - 0 7",
        "3r2k1/pp3ppp/2n1b3/3p4/3P4/2P1BN2/PP3PPP/3R2K1 w - - 0 1"};

    sirio::SearchLimits speed_limits;
    speed_limits.max_depth = 4;

    std::uint64_t total_nodes = 0;
    auto speed_start = std::chrono::steady_clock::now();
    for (const auto& fen : speed_positions) {
        sirio::Board bench_board{fen};
        auto result = sirio::search_best_move(bench_board, speed_limits);
        total_nodes += result.nodes;
    }
    auto speed_end = std::chrono::steady_clock::now();
    auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(speed_end - speed_start);
    double seconds = static_cast<double>(elapsed_ms.count()) / 1000.0;
    std::uint64_t nps = seconds > 0.0
                            ? static_cast<std::uint64_t>(static_cast<double>(total_nodes) / seconds)
                            : 0ULL;

    log("Search speed benchmark:");
    log("  Positions: " + std::to_string(speed_positions.size()));
    log("  Time: " + std::to_string(elapsed_ms.count()) + " ms");
    log("  Nodes: " + std::to_string(total_nodes));
    log("  Nodes per second: " + std::to_string(nps));

    struct TacticalPosition {
        std::string fen;
        std::string best_move;
    };

    std::vector<TacticalPosition> tactical_suite = {
        {"6k1/5ppp/8/6Q1/8/8/8/6K1 w - - 0 1", "g5d8"},
        {"k7/8/8/8/8/8/5PPP/6KQ w - - 0 1", "g2g4"}};

    sirio::SearchLimits tactic_limits;
    tactic_limits.max_depth = 1;
    tactic_limits.move_time = 1000;

    int correct = 0;
    std::vector<std::string> mismatch_logs;
    for (const auto& entry : tactical_suite) {
        sirio::Board bench_board{entry.fen};
        auto result = sirio::search_best_move(bench_board, tactic_limits);
        std::string uci = result.has_move ? sirio::move_to_uci(result.best_move) : "(none)";
        if (result.has_move && uci == entry.best_move) {
            ++correct;
        } else {
            mismatch_logs.push_back("  " + entry.fen + " -> esperado " + entry.best_move +
                                   ", obtenido " + uci);
        }
    }

    log("Tactical suite accuracy: " + std::to_string(correct) + "/" +
        std::to_string(tactical_suite.size()));
    for (const auto& line : mismatch_logs) {
        log(line);
    }

    const std::string& tb_path = sirio::syzygy::tablebase_path();
    if (!tb_path.empty() && sirio::syzygy::available()) {
        sirio::Board tb_board{"8/8/8/8/8/6k1/6P1/6K1 w - - 0 1"};
        if (auto probe = sirio::syzygy::probe_root(tb_board); probe.has_value() &&
                                                             probe->best_move.has_value()) {
            log(std::string{"Syzygy probe move: "} +
                sirio::move_to_uci(*probe->best_move) + " (wdl=" +
                std::to_string(probe->wdl) + ", dtz=" + std::to_string(probe->dtz) + ")");
        }
    } else {
        log("Syzygy tablebases no configuradas. Establezca la opción SyzygyPath para habilitar "
            "las pruebas de tablebases.");
    }

    log("Bench finalizado");
}

}  // namespace

int run() {
    sirio::Board board;
    sirio::initialize_evaluation(board);
    initialize_engine_options();
    ensure_options_registered();
    sync_options_from_state();

    std::string line;

    while (std::getline(std::cin, line)) {
        std::string trimmed = trim_leading(line);
        if (trimmed.empty()) {
            continue;
        }

        std::istringstream stream{trimmed};
        std::string command;
        stream >> command;

        try {
            if (command == "uci") {
                send_uci_id();
            } else if (command == "isready") {
                stop_and_join_search();
                send_ready(board);
            } else if (command == "ucinewgame") {
                stop_and_join_search();
                board = sirio::Board{};
                sirio::initialize_evaluation(board);
            } else if (command == "position") {
                std::string rest;
                stop_and_join_search();
                std::getline(stream, rest);
                set_position(board, rest);
            } else if (command == "go") {
                std::string rest;
                std::getline(stream, rest);
                handle_go(rest, board);
            } else if (command == "setoption") {
                std::string rest;
                stop_and_join_search();
                std::getline(stream, rest);
                if (!handle_setoption(rest, board)) {
                    std::cout << "info string Unknown option: " << trim_whitespace(rest) << std::endl;
                }
            } else if (command == "bench") {
                stop_and_join_search();
                handle_bench();
            } else if (command == "stop") {
                stop_and_join_search();
            } else if (command == "quit") {
                stop_and_join_search();
                save_persistent_analysis_if_enabled(true);
                break;
            } else if (command == "d") {
                stop_and_join_search();
                std::cout << board.to_fen() << std::endl;
            }
        } catch (const std::exception& ex) {
            std::cerr << "Error: " << ex.what() << std::endl;
        }
    }

    stop_and_join_search();
    save_persistent_analysis_if_enabled(false);
    return 0;
}

}  // namespace uci
}  // namespace sirio

