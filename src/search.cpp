#include "sirio/search.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include "sirio/draws.hpp"
#include "sirio/endgame.hpp"
#include "sirio/evaluation.hpp"
#include "sirio/move.hpp"
#include "sirio/movegen.hpp"
#include "sirio/syzygy.hpp"

namespace sirio {

namespace {

struct SearchSharedState;

constexpr int mate_score = 100000;
constexpr int max_search_depth = 64;
constexpr int mate_threshold = mate_score - max_search_depth;
constexpr std::size_t default_tt_size_mb = 16;

constexpr std::array<int, static_cast<std::size_t>(PieceType::Count)> mvv_values = {
    100, 320, 330, 500, 900, 20000};

enum class TTNodeType { Exact, LowerBound, UpperBound };

std::atomic<int> search_thread_count{1};
std::atomic<std::size_t> transposition_table_size_mb{default_tt_size_mb};
std::atomic<std::uint64_t> transposition_table_epoch{1};
std::atomic<int> time_move_overhead{10};
std::atomic<int> time_minimum_thinking{100};
std::atomic<int> time_slow_mover{100};
std::atomic<int> time_nodes_per_ms{0};

std::mutex active_search_mutex;
SearchSharedState *active_search_state = nullptr;
std::mutex info_output_mutex;

class EvaluationStateGuard {
public:
    EvaluationStateGuard() = default;
    explicit EvaluationStateGuard(bool active) : active_(active) {}
    EvaluationStateGuard(const EvaluationStateGuard &) = delete;
    EvaluationStateGuard &operator=(const EvaluationStateGuard &) = delete;
    EvaluationStateGuard(EvaluationStateGuard &&other) noexcept
        : active_(other.active_) {
        other.active_ = false;
    }
    EvaluationStateGuard &operator=(EvaluationStateGuard &&other) noexcept {
        if (this != &other) {
            if (active_) {
                pop_evaluation_state();
            }
            active_ = other.active_;
            other.active_ = false;
        }
        return *this;
    }
    ~EvaluationStateGuard() {
        if (active_) {
            pop_evaluation_state();
        }
    }
    void release() { active_ = false; }

private:
    bool active_ = false;
};

struct TTEntry {
    Move best_move{};
    int depth = 0;
    int score = 0;
    TTNodeType type = TTNodeType::Exact;
    int static_eval = 0;
    std::uint8_t generation = 0;
};

struct PackedTTEntry {
    std::uint64_t key = 0;
    std::int32_t score = 0;
    std::int16_t depth = -1;
    std::int32_t static_eval = 0;
    std::uint8_t type = 0;
    std::uint8_t generation = 0;
    std::uint8_t from = 0;
    std::uint8_t to = 0;
    std::uint8_t piece = 0;
    std::uint8_t captured = 0xFF;
    std::uint8_t promotion = 0xFF;
    std::uint8_t flags = 0;
};

constexpr std::size_t tt_mutex_shards = 64;

Move unpack_move(const PackedTTEntry &slot) {
    Move move{};
    move.from = slot.from;
    move.to = slot.to;
    move.piece = static_cast<PieceType>(slot.piece);
    if (slot.captured != 0xFF) {
        move.captured = static_cast<PieceType>(slot.captured);
    }
    if (slot.promotion != 0xFF) {
        move.promotion = static_cast<PieceType>(slot.promotion);
    }
    move.is_en_passant = (slot.flags & 0x1) != 0;
    move.is_castling = (slot.flags & 0x2) != 0;
    return move;
}

void pack_move(const Move &move, PackedTTEntry &slot) {
    slot.from = static_cast<std::uint8_t>(move.from);
    slot.to = static_cast<std::uint8_t>(move.to);
    slot.piece = static_cast<std::uint8_t>(move.piece);
    slot.captured = move.captured.has_value() ? static_cast<std::uint8_t>(*move.captured) : 0xFF;
    slot.promotion = move.promotion.has_value() ? static_cast<std::uint8_t>(*move.promotion) : 0xFF;
    slot.flags = (move.is_en_passant ? 0x1 : 0x0) | (move.is_castling ? 0x2 : 0x0);
}

class GlobalTranspositionTable {
public:
    std::uint8_t prepare_for_search() {
        std::unique_lock lock(global_mutex_);
        ensure_settings_locked();
        ++generation_;
        if (generation_ == 0) {
            generation_ = 1;
        }
        return generation_;
    }

    void store(std::uint64_t key, const TTEntry &entry, std::uint8_t generation) {
        std::shared_lock lock(global_mutex_);
        if (entries_.empty()) {
            return;
        }
        const std::size_t index = static_cast<std::size_t>(key & (entries_.size() - 1));
        std::lock_guard shard_lock(shard_mutexes_[index % tt_mutex_shards]);
        PackedTTEntry &slot = entries_[index];
        if (slot.depth < entry.depth || slot.key != key || slot.generation != generation ||
            entry.type == TTNodeType::Exact || slot.depth < 0) {
            slot.key = key;
            pack_move(entry.best_move, slot);
            slot.score = entry.score;
            slot.depth = static_cast<std::int16_t>(entry.depth);
            slot.static_eval = entry.static_eval;
            slot.type = static_cast<std::uint8_t>(entry.type);
            slot.generation = generation;
        }
    }

    std::optional<TTEntry> probe(std::uint64_t key) const {
        std::shared_lock lock(global_mutex_);
        if (entries_.empty()) {
            return std::nullopt;
        }
        const std::size_t index = static_cast<std::size_t>(key & (entries_.size() - 1));
        std::lock_guard shard_lock(shard_mutexes_[index % tt_mutex_shards]);
        const PackedTTEntry &slot = entries_[index];
        if (slot.depth < 0 || slot.key != key) {
            return std::nullopt;
        }
        TTEntry entry;
        entry.best_move = unpack_move(slot);
        entry.score = slot.score;
        entry.depth = slot.depth;
        entry.type = static_cast<TTNodeType>(slot.type);
        entry.static_eval = slot.static_eval;
        entry.generation = slot.generation;
        return entry;
    }

    bool save(const std::string &path, std::string *error) const {
        std::unique_lock lock(global_mutex_);
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) {
            if (error != nullptr) {
                *error = "No se pudo guardar la tabla de transposición: " + path;
            }
            return false;
        }
        const char magic[4] = {'S', 'R', 'T', 'T'};
        const std::uint32_t version = 1;
        const std::uint64_t count = static_cast<std::uint64_t>(entries_.size());
        out.write(magic, sizeof(magic));
        out.write(reinterpret_cast<const char *>(&version), sizeof(version));
        out.write(reinterpret_cast<const char *>(&count), sizeof(count));
        out.write(reinterpret_cast<const char *>(&configured_size_mb_), sizeof(configured_size_mb_));
        out.write(reinterpret_cast<const char *>(&generation_), sizeof(generation_));
        for (const auto &slot : entries_) {
            out.write(reinterpret_cast<const char *>(&slot.key), sizeof(slot.key));
            out.write(reinterpret_cast<const char *>(&slot.score), sizeof(slot.score));
            out.write(reinterpret_cast<const char *>(&slot.depth), sizeof(slot.depth));
            out.write(reinterpret_cast<const char *>(&slot.static_eval), sizeof(slot.static_eval));
            out.write(reinterpret_cast<const char *>(&slot.type), sizeof(slot.type));
            out.write(reinterpret_cast<const char *>(&slot.generation), sizeof(slot.generation));
            out.write(reinterpret_cast<const char *>(&slot.from), sizeof(slot.from));
            out.write(reinterpret_cast<const char *>(&slot.to), sizeof(slot.to));
            out.write(reinterpret_cast<const char *>(&slot.piece), sizeof(slot.piece));
            out.write(reinterpret_cast<const char *>(&slot.captured), sizeof(slot.captured));
            out.write(reinterpret_cast<const char *>(&slot.promotion), sizeof(slot.promotion));
            out.write(reinterpret_cast<const char *>(&slot.flags), sizeof(slot.flags));
        }
        if (!out.good() && error != nullptr) {
            *error = "Error al escribir la tabla de transposición";
        }
        return out.good();
    }

    bool load(const std::string &path, std::string *error) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            if (error != nullptr) {
                *error = "No se pudo abrir la tabla de transposición: " + path;
            }
            return false;
        }
        char magic[4];
        std::uint32_t version = 0;
        std::uint64_t count = 0;
        std::size_t size_mb = 0;
        std::uint8_t saved_generation = 1;
        in.read(magic, sizeof(magic));
        in.read(reinterpret_cast<char *>(&version), sizeof(version));
        in.read(reinterpret_cast<char *>(&count), sizeof(count));
        in.read(reinterpret_cast<char *>(&size_mb), sizeof(size_mb));
        in.read(reinterpret_cast<char *>(&saved_generation), sizeof(saved_generation));
        if (!in.good() || std::string_view(magic, sizeof(magic)) != "SRTT" || version != 1) {
            if (error != nullptr) {
                *error = "Formato de tabla de transposición inválido";
            }
            return false;
        }
        std::vector<PackedTTEntry> new_entries(static_cast<std::size_t>(count));
        for (std::size_t i = 0; i < new_entries.size(); ++i) {
            PackedTTEntry slot;
            in.read(reinterpret_cast<char *>(&slot.key), sizeof(slot.key));
            in.read(reinterpret_cast<char *>(&slot.score), sizeof(slot.score));
            in.read(reinterpret_cast<char *>(&slot.depth), sizeof(slot.depth));
            in.read(reinterpret_cast<char *>(&slot.static_eval), sizeof(slot.static_eval));
            in.read(reinterpret_cast<char *>(&slot.type), sizeof(slot.type));
            in.read(reinterpret_cast<char *>(&slot.generation), sizeof(slot.generation));
            in.read(reinterpret_cast<char *>(&slot.from), sizeof(slot.from));
            in.read(reinterpret_cast<char *>(&slot.to), sizeof(slot.to));
            in.read(reinterpret_cast<char *>(&slot.piece), sizeof(slot.piece));
            in.read(reinterpret_cast<char *>(&slot.captured), sizeof(slot.captured));
            in.read(reinterpret_cast<char *>(&slot.promotion), sizeof(slot.promotion));
            in.read(reinterpret_cast<char *>(&slot.flags), sizeof(slot.flags));
            if (!in.good()) {
                if (error != nullptr) {
                    *error = "Archivo de tabla de transposición truncado";
                }
                return false;
            }
            new_entries[i] = slot;
        }

        std::unique_lock lock(global_mutex_);
        entries_ = std::move(new_entries);
        configured_size_mb_ = size_mb;
        generation_ = saved_generation == 0 ? 1 : saved_generation;
        epoch_marker_ = transposition_table_epoch.load(std::memory_order_relaxed);
        transposition_table_size_mb.store(size_mb, std::memory_order_relaxed);
        return true;
    }

private:
    void ensure_settings_locked() {
        const std::size_t desired =
            transposition_table_size_mb.load(std::memory_order_relaxed);
        const std::uint64_t epoch =
            transposition_table_epoch.load(std::memory_order_relaxed);
        if (configured_size_mb_ != desired || epoch_marker_ != epoch) {
            rebuild_locked(desired);
            epoch_marker_ = epoch;
        }
    }

    void rebuild_locked(std::size_t size_mb) {
        configured_size_mb_ = size_mb;
        if (size_mb == 0) {
            entries_.clear();
            generation_ = 1;
            return;
        }
        std::size_t bytes = size_mb * 1024ULL * 1024ULL;
        std::size_t count = std::max<std::size_t>(1, bytes / sizeof(PackedTTEntry));
        std::size_t pow2 = std::bit_ceil(count);
        entries_.assign(pow2, PackedTTEntry{});
        generation_ = 1;
    }

    mutable std::shared_mutex global_mutex_;
    mutable std::array<std::mutex, tt_mutex_shards> shard_mutexes_{};
    std::vector<PackedTTEntry> entries_;
    std::uint8_t generation_ = 1;
    std::size_t configured_size_mb_ = 0;
    std::uint64_t epoch_marker_ = 0;
};

GlobalTranspositionTable &shared_transposition_table() {
    static GlobalTranspositionTable table;
    return table;
}

struct SearchSharedState {
    std::atomic<bool> stop{false};
    std::atomic<bool> soft_limit_reached{false};
    std::atomic<bool> timed_out{false};
    std::atomic<std::uint64_t> node_counter{0};
    bool has_time_limit = false;
    bool has_node_limit = false;
    std::chrono::steady_clock::time_point start_time{};
    std::chrono::milliseconds soft_time_limit{0};
    std::chrono::milliseconds hard_time_limit{0};
    std::uint64_t node_limit = 0;
};

struct SearchContext {
    std::array<std::array<std::optional<Move>, 2>, max_search_depth> killer_moves{};
    GlobalTranspositionTable *tt = nullptr;
    std::uint8_t tt_generation = 1;
    SearchSharedState *shared = nullptr;
    std::chrono::milliseconds last_iteration_time{0};
    int selective_depth = 0;
};

class ActiveSearchGuard {
public:
    explicit ActiveSearchGuard(SearchSharedState *state) : state_(state) {
        std::lock_guard<std::mutex> lock(active_search_mutex);
        active_search_state = state_;
    }

    ~ActiveSearchGuard() {
        std::lock_guard<std::mutex> lock(active_search_mutex);
        if (active_search_state == state_) {
            active_search_state = nullptr;
        }
    }

    ActiveSearchGuard(const ActiveSearchGuard &) = delete;
    ActiveSearchGuard &operator=(const ActiveSearchGuard &) = delete;

private:
    SearchSharedState *state_;
};

constexpr std::uint64_t time_check_interval = 2048;

}  // namespace

void set_search_threads(int threads) {
    int clamped = std::clamp(threads, 1, 1024);
    search_thread_count.store(clamped, std::memory_order_relaxed);
}

int get_search_threads() { return search_thread_count.load(std::memory_order_relaxed); }

void set_transposition_table_size(std::size_t size_mb) {
    size_mb = std::clamp<std::size_t>(size_mb, 1, 33'554'432);
    transposition_table_size_mb.store(size_mb, std::memory_order_relaxed);
    transposition_table_epoch.fetch_add(1, std::memory_order_relaxed);
}

std::size_t get_transposition_table_size() {
    return transposition_table_size_mb.load(std::memory_order_relaxed);
}

void clear_transposition_tables() {
    transposition_table_epoch.fetch_add(1, std::memory_order_relaxed);
}

void set_move_overhead(int milliseconds) {
    time_move_overhead.store(std::clamp(milliseconds, 0, 5000), std::memory_order_relaxed);
}

void set_minimum_thinking_time(int milliseconds) {
    time_minimum_thinking.store(std::clamp(milliseconds, 0, 5000), std::memory_order_relaxed);
}

void set_slow_mover(int value) {
    time_slow_mover.store(std::clamp(value, 10, 1000), std::memory_order_relaxed);
}

void set_nodestime(int value) {
    time_nodes_per_ms.store(std::clamp(value, 0, 10000), std::memory_order_relaxed);
}

int get_move_overhead() { return time_move_overhead.load(std::memory_order_relaxed); }

int get_minimum_thinking_time() {
    return time_minimum_thinking.load(std::memory_order_relaxed);
}

int get_slow_mover() { return time_slow_mover.load(std::memory_order_relaxed); }

int get_nodestime() { return time_nodes_per_ms.load(std::memory_order_relaxed); }

namespace {

int total_piece_count(const Board &board) {
    int total = 0;
    for (int color_index = 0; color_index < 2; ++color_index) {
        Color color = color_index == 0 ? Color::White : Color::Black;
        for (int type_index = 0; type_index < static_cast<int>(PieceType::Count); ++type_index) {
            total += std::popcount(board.pieces(color, static_cast<PieceType>(type_index)));
        }
    }
    return total;
}

bool has_non_pawn_material(const Board &board, Color color) {
    return (board.pieces(color, PieceType::Queen) | board.pieces(color, PieceType::Rook) |
            board.pieces(color, PieceType::Bishop) | board.pieces(color, PieceType::Knight)) != 0;
}

int syzygy_wdl_to_score(int wdl, int ply) {
    switch (wdl) {
        case 2:
            return mate_score - ply;
        case 1:
            return 200;
        case 0:
            return 0;
        case -1:
            return -200;
        case -2:
        default:
            return -mate_score + ply;
    }
}

bool same_move(const Move &lhs, const Move &rhs) {
    return lhs.from == rhs.from && lhs.to == rhs.to && lhs.piece == rhs.piece &&
           lhs.captured == rhs.captured && lhs.promotion == rhs.promotion &&
           lhs.is_en_passant == rhs.is_en_passant &&
           lhs.is_castling == rhs.is_castling;
}

std::vector<Move> extract_principal_variation(const Board &board, GlobalTranspositionTable &tt,
                                              std::uint8_t generation, int max_length) {
    std::vector<Move> pv;
    if (max_length <= 0) {
        return pv;
    }
    Board current = board;
    std::unordered_set<std::uint64_t> visited;
    visited.insert(current.zobrist_hash());
    const int limit = std::min(max_length, max_search_depth);
    for (int depth = 0; depth < limit; ++depth) {
        auto entry_opt = tt.probe(current.zobrist_hash());
        if (!entry_opt.has_value()) {
            break;
        }
        const TTEntry &entry = *entry_opt;
        if (entry.generation != 0 && entry.generation != generation) {
            break;
        }
        auto moves = generate_legal_moves(current);
        auto it = std::find_if(moves.begin(), moves.end(),
                               [&](const Move &candidate) {
                                   return same_move(candidate, entry.best_move);
                               });
        if (it == moves.end()) {
            break;
        }
        pv.push_back(*it);
        current = current.apply_move(*it);
        std::uint64_t hash = current.zobrist_hash();
        if (visited.count(hash) != 0) {
            break;
        }
        visited.insert(hash);
    }
    return pv;
}

void announce_search_update(const Board &board, const SearchResult &result,
                            const SearchSharedState &shared_state) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - shared_state.start_time);
    long long elapsed_ms = elapsed.count();
    if (elapsed_ms < 0) {
        elapsed_ms = 0;
    }
    std::uint64_t nodes = shared_state.node_counter.load(std::memory_order_relaxed);
    std::uint64_t nps = elapsed_ms > 0 ? (nodes * 1000ULL) / static_cast<std::uint64_t>(elapsed_ms) : 0ULL;
    int depth = result.depth_reached;
    int seldepth = result.seldepth > 0 ? result.seldepth : depth;
    std::string pv_string = principal_variation_to_uci(board, result.principal_variation);
    std::lock_guard<std::mutex> lock(info_output_mutex);
    std::cout << "info depth " << depth << " seldepth " << seldepth << " multipv 1 score "
              << format_uci_score(result.score) << " nodes " << nodes << " nps " << nps
              << " hashfull 0 tbhits 0 time " << elapsed_ms;
    if (!pv_string.empty()) {
        std::cout << " pv " << pv_string;
    }
    std::cout << std::endl;
}

bool is_quiet(const Move &move) {
    return !move.captured.has_value() && !move.promotion.has_value() && !move.is_castling &&
           !move.is_en_passant;
}

int mvv_lva_score(const Move &move) {
    if (!move.captured.has_value()) {
        return 0;
    }
    auto victim_index = static_cast<std::size_t>(*move.captured);
    auto attacker_index = static_cast<std::size_t>(move.piece);
    return mvv_values[victim_index] * 100 - mvv_values[attacker_index];
}

int killer_score(const Move &move, const SearchContext &context, int ply) {
    if (ply >= max_search_depth) {
        return 0;
    }
    const auto &slots = context.killer_moves[static_cast<std::size_t>(ply)];
    for (std::size_t index = 0; index < slots.size(); ++index) {
        if (slots[index].has_value() && same_move(*slots[index], move)) {
            return 800000 - static_cast<int>(index);
        }
    }
    return 0;
}

int score_move(const Move &move, const SearchContext &context, int ply,
               const std::optional<Move> &tt_move) {
    if (tt_move.has_value() && same_move(*tt_move, move)) {
        return 1'000'000;
    }
    if (move.captured.has_value()) {
        return 900000 + mvv_lva_score(move);
    }
    return killer_score(move, context, ply);
}

bool should_stop(SearchContext &context) {
    SearchSharedState &shared = *context.shared;
    std::uint64_t nodes = shared.node_counter.fetch_add(1, std::memory_order_relaxed) + 1;
    if (shared.stop.load(std::memory_order_relaxed)) {
        return true;
    }
    if (shared.has_node_limit && nodes >= shared.node_limit) {
        shared.stop.store(true, std::memory_order_relaxed);
        return true;
    }
    if (!shared.has_time_limit) {
        return false;
    }
    if ((nodes & (time_check_interval - 1)) != 0) {
        return false;
    }
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - shared.start_time);
    if (!shared.soft_limit_reached.load(std::memory_order_relaxed) &&
        elapsed >= shared.soft_time_limit) {
        shared.soft_limit_reached.store(true, std::memory_order_relaxed);
    }
    if (elapsed >= shared.hard_time_limit) {
        shared.stop.store(true, std::memory_order_relaxed);
        shared.timed_out.store(true, std::memory_order_relaxed);
        return true;
    }
    return shared.stop.load(std::memory_order_relaxed);
}

int evaluate_for_current_player(const Board &board) {
    int eval = evaluate(board);
    return board.side_to_move() == Color::White ? eval : -eval;
}

int to_tt_score(int score, int ply) {
    if (score > mate_threshold) {
        return score + ply;
    }
    if (score < -mate_threshold) {
        return score - ply;
    }
    return score;
}

int from_tt_score(int score, int ply) {
    if (score > mate_threshold) {
        return score - ply;
    }
    if (score < -mate_threshold) {
        return score + ply;
    }
    return score;
}

void order_moves(std::vector<Move> &moves, const SearchContext &context, int ply,
                 const std::optional<Move> &tt_move) {
    std::vector<std::pair<int, Move>> scored;
    scored.reserve(moves.size());
    for (const Move &move : moves) {
        scored.emplace_back(score_move(move, context, ply, tt_move), move);
    }
    std::stable_sort(scored.begin(), scored.end(),
                     [](const auto &lhs, const auto &rhs) { return lhs.first > rhs.first; });
    moves.clear();
    moves.reserve(scored.size());
    for (auto &entry : scored) {
        moves.push_back(entry.second);
    }
}

void store_killer(const Move &move, SearchContext &context, int ply) {
    if (!is_quiet(move) || ply >= max_search_depth) {
        return;
    }
    auto &slots = context.killer_moves[static_cast<std::size_t>(ply)];
    if (!slots[0].has_value() || !same_move(*slots[0], move)) {
        slots[1] = slots[0];
        slots[0] = move;
    }
}

int quiescence(const Board &board, int alpha, int beta, int ply, SearchContext &context);

int negamax(const Board &board, int depth, int alpha, int beta, int ply, Move *best_move,
            bool *found_best, SearchContext &context, int parent_static_eval,
            bool allow_null_move) {
    context.selective_depth = std::max(context.selective_depth, ply + 1);
    if (should_stop(context)) {
        return evaluate_for_current_player(board);
    }
    if (!sufficient_material_to_force_checkmate(board)) {
        return 0;
    }

    if (draw_by_fifty_move_rule(board) || draw_by_threefold_repetition(board) ||
        draw_by_insufficient_material_rule(board)) {
        return 0;
    }

    const std::uint64_t hash = board.zobrist_hash();
    bool in_check = board.in_check(board.side_to_move());
    int static_eval = 0;
    if (!in_check) {
        static_eval = evaluate_for_current_player(board);
    }

    const int max_remaining_depth = max_search_depth - ply;
    int depth_left = std::min(depth, max_remaining_depth);
    if (in_check && depth_left < max_remaining_depth) {
        ++depth_left;
    }

    std::optional<TTEntry> tt_entry;
    if (context.tt != nullptr) {
        tt_entry = context.tt->probe(hash);
    }
    std::optional<Move> tt_move;
    if (tt_entry.has_value()) {
        tt_move = tt_entry->best_move;
        if (!in_check && tt_entry->static_eval != 0) {
            static_eval = tt_entry->static_eval;
        }
    }

    int piece_count = total_piece_count(board);
    if (syzygy::available() && piece_count <= syzygy::probe_piece_limit() &&
        syzygy::max_pieces() >= piece_count && depth_left <= syzygy::probe_depth_limit()) {
        if (auto tb = syzygy::probe_wdl(board); tb.has_value()) {
            int tb_score = syzygy_wdl_to_score(tb->wdl, ply);
            if (std::abs(tb_score) >= mate_threshold || tb->wdl == 0) {
                if (best_move && tb->best_move) {
                    *best_move = *tb->best_move;
                }
                if (found_best && tb->best_move) {
                    *found_best = true;
                }
                return tb_score;
            }
            if (!in_check) {
                static_eval = tb_score;
            }
        }
    }

    if (depth_left <= 0) {
        return quiescence(board, alpha, beta, ply, context);
    }

    if (tt_entry.has_value() && tt_entry->depth >= depth_left) {
        int tt_score = from_tt_score(tt_entry->score, ply);
        switch (tt_entry->type) {
            case TTNodeType::Exact:
                if (best_move) {
                    *best_move = tt_entry->best_move;
                }
                if (found_best) {
                    *found_best = true;
                }
                return tt_score;
            case TTNodeType::LowerBound:
                alpha = std::max(alpha, tt_score);
                break;
            case TTNodeType::UpperBound:
                beta = std::min(beta, tt_score);
                break;
        }
        if (alpha >= beta) {
            if (best_move) {
                *best_move = tt_entry->best_move;
            }
            if (found_best) {
                *found_best = true;
            }
            return tt_score;
        }
    }

    if (!in_check && depth_left == 1) {
        constexpr int futility_margin = 150;
        if (static_eval - futility_margin >= beta) {
            return static_eval - futility_margin;
        }
    }

    if (allow_null_move && !in_check && depth_left >= 3 && static_eval >= beta &&
        has_non_pawn_material(board, board.side_to_move())) {
        Board null_board = board.apply_null_move();
        EvaluationStateGuard null_guard{true};
        int reduction = 2 + depth_left / 4;
        int null_depth = depth_left - 1 - reduction;
        if (null_depth >= 0) {
            int null_score = -negamax(null_board, null_depth, -beta, -beta + 1, ply + 1, nullptr,
                                      nullptr, context, static_eval, false);
            if (context.shared->stop.load(std::memory_order_relaxed)) {
                return 0;
            }
            if (null_score >= beta) {
                return beta;
            }
        }
    }

    auto moves = generate_legal_moves(board);
    if (moves.empty()) {
        if (in_check) {
            return -mate_score + ply;
        }
        return 0;
    }

    order_moves(moves, context, ply, tt_move);

    int alpha_original = alpha;
    int best_score = std::numeric_limits<int>::min();
    Move local_best{};
    bool local_found = false;

    int move_index = 0;
    for (const Move &move : moves) {
        ++move_index;
        Board next = board.apply_move(move);
        EvaluationStateGuard eval_guard{true};
        bool gives_check = next.in_check(next.side_to_move());

        int child_depth = depth_left - 1;
        if (gives_check && child_depth < max_search_depth - (ply + 1)) {
            ++child_depth;
        }
        child_depth = std::min(child_depth, std::max(0, max_search_depth - (ply + 1)));

        int reduction = 0;
        if (child_depth > 0 && depth_left >= 3 && move_index > 1 && is_quiet(move) && !gives_check) {
            bool improving = static_eval > parent_static_eval;
            reduction = 1;
            if (depth_left >= 5 && move_index > 4) {
                ++reduction;
            }
            if (!improving) {
                ++reduction;
            }
            if (reduction > child_depth - 1) {
                reduction = child_depth - 1;
            }
            if (reduction < 0) {
                reduction = 0;
            }
        }

        int new_depth = std::max(0, child_depth - reduction);
        int score;
        if (new_depth <= 0) {
            score = -quiescence(next, -beta, -alpha, ply + 1, context);
        } else {
            score = -negamax(next, new_depth, -beta, -alpha, ply + 1, nullptr, nullptr, context,
                             static_eval, true);
        }
        if (context.shared->stop.load(std::memory_order_relaxed)) {
            return 0;
        }

        if (score > best_score) {
            best_score = score;
            local_best = move;
            local_found = true;
        }
        if (score > alpha) {
            alpha = score;
        }
        if (alpha >= beta) {
            if (is_quiet(move)) {
                store_killer(move, context, ply);
            }
            break;
        }
    }

    if (best_move && local_found) {
        *best_move = local_best;
    }
    if (found_best) {
        *found_best = local_found;
    }

    if (context.shared->stop.load(std::memory_order_relaxed)) {
        return best_score;
    }

    if (local_found) {
        TTEntry new_entry{};
        new_entry.best_move = local_best;
        new_entry.depth = depth_left;
        new_entry.score = to_tt_score(best_score, ply);
        new_entry.static_eval = static_eval;
        if (best_score <= alpha_original) {
            new_entry.type = TTNodeType::UpperBound;
        } else if (best_score >= beta) {
            new_entry.type = TTNodeType::LowerBound;
        } else {
            new_entry.type = TTNodeType::Exact;
        }
        if (context.tt != nullptr) {
            context.tt->store(hash, new_entry, context.tt_generation);
        }
    }

    return best_score;
}

int quiescence(const Board &board, int alpha, int beta, int ply, SearchContext &context) {
    context.selective_depth = std::max(context.selective_depth, ply + 1);
    if (should_stop(context)) {
        return alpha;
    }
    if (syzygy::available() && syzygy::max_pieces() >= total_piece_count(board)) {
        if (auto tb = syzygy::probe_wdl(board); tb.has_value()) {
            return syzygy_wdl_to_score(tb->wdl, ply);
        }
    }
    int stand_pat = evaluate_for_current_player(board);
    if (stand_pat >= beta) {
        return stand_pat;
    }
    if (stand_pat > alpha) {
        alpha = stand_pat;
    }

    auto moves = generate_legal_moves(board);
    std::vector<Move> tactical_moves;
    tactical_moves.reserve(moves.size());
    for (const Move &move : moves) {
        if (move.captured.has_value() || move.is_en_passant || move.promotion.has_value()) {
            tactical_moves.push_back(move);
        }
    }

    if (tactical_moves.empty()) {
        return alpha;
    }

    order_moves(tactical_moves, context, ply, std::nullopt);

    for (const Move &move : tactical_moves) {
        Board next = board.apply_move(move);
        EvaluationStateGuard eval_guard{true};
        int score = -quiescence(next, -beta, -alpha, ply + 1, context);
        if (context.shared->stop.load(std::memory_order_relaxed)) {
            return alpha;
        }
        if (score >= beta) {
            return score;
        }
        if (score > alpha) {
            alpha = score;
        }
    }

    return alpha;
}

}  // namespace

struct TimeAllocation {
    std::chrono::milliseconds soft;
    std::chrono::milliseconds hard;
};

void adjust_time_allocation(std::chrono::milliseconds &soft, std::chrono::milliseconds &hard) {
    const int overhead = std::clamp(time_move_overhead.load(std::memory_order_relaxed), 0, 5000);
    const int min_thinking =
        std::clamp(time_minimum_thinking.load(std::memory_order_relaxed), 0, 5000);
    const int slow = std::clamp(time_slow_mover.load(std::memory_order_relaxed), 10, 1000);
    const auto overhead_ms = std::chrono::milliseconds{overhead};
    const auto minimum_ms = std::chrono::milliseconds{min_thinking};

    auto adjust_single = [&](std::chrono::milliseconds value) {
        if (value <= overhead_ms) {
            value = std::chrono::milliseconds{0};
        } else {
            value -= overhead_ms;
        }
        long long scaled = value.count() * slow / 100;
        if (scaled < minimum_ms.count()) {
            scaled = minimum_ms.count();
        }
        if (scaled <= 0) {
            scaled = minimum_ms.count() > 0 ? minimum_ms.count() : 1;
        }
        return std::chrono::milliseconds{scaled};
    };

    hard = adjust_single(hard);
    soft = adjust_single(soft);
    if (soft > hard) {
        soft = hard;
    }
}

std::uint64_t nodes_budget_for_time(std::chrono::milliseconds hard) {
    const int nodes_per_ms = time_nodes_per_ms.load(std::memory_order_relaxed);
    if (nodes_per_ms <= 0 || hard.count() <= 0) {
        return 0;
    }
    return static_cast<std::uint64_t>(nodes_per_ms) *
           static_cast<std::uint64_t>(hard.count());
}

std::optional<TimeAllocation> compute_time_allocation(const Board &board,
                                                      const SearchLimits &limits) {
    if (limits.move_time > 0) {
        auto hard = std::chrono::milliseconds{limits.move_time};
        auto soft = std::chrono::milliseconds{std::max<int>(1, limits.move_time * 9 / 10)};
        if (soft > hard) {
            soft = hard;
        }
        adjust_time_allocation(soft, hard);
        return TimeAllocation{soft, hard};
    }

    Color stm = board.side_to_move();
    int time_left = stm == Color::White ? limits.time_left_white : limits.time_left_black;
    int increment = stm == Color::White ? limits.increment_white : limits.increment_black;
    int moves_to_go = limits.moves_to_go > 0 ? limits.moves_to_go : 30;

    if (time_left > 0) {
        int allocation = time_left / std::max(1, moves_to_go);
        if (increment > 0) {
            allocation += increment;
        }
        allocation = std::max(allocation, increment);
        int reserve = std::max(1, time_left / 20);
        int max_allocation = time_left - reserve;
        if (max_allocation <= 0) {
            max_allocation = std::max(1, time_left / 2);
        }
        allocation = std::min(allocation, max_allocation);
        allocation = std::max(allocation, 1);
        auto hard = std::chrono::milliseconds{allocation};
        auto soft = std::chrono::milliseconds{std::max(1, allocation * 9 / 10)};
        if (soft > hard) {
            soft = hard;
        }
        adjust_time_allocation(soft, hard);
        return TimeAllocation{soft, hard};
    }

    if (increment > 0) {
        int allocation = std::max(increment / 2, 1);
        auto hard = std::chrono::milliseconds{allocation};
        auto soft = hard;
        adjust_time_allocation(soft, hard);
        return TimeAllocation{soft, hard};
    }

    return std::nullopt;
}

struct SharedBestResult {
    std::mutex mutex;
    SearchResult result;
};

bool publish_best_result(const SearchResult &candidate, SharedBestResult &shared,
                         const Board &board, GlobalTranspositionTable &tt,
                         std::uint8_t tt_generation, SearchSharedState &shared_state,
                         bool announce_update) {
    if (!candidate.has_move) {
        return false;
    }

    bool should_update = false;
    {
        std::lock_guard lock(shared.mutex);
        if (!shared.result.has_move ||
            candidate.depth_reached > shared.result.depth_reached ||
            (candidate.depth_reached == shared.result.depth_reached &&
             std::abs(candidate.score) >= std::abs(shared.result.score))) {
            should_update = true;
        }
    }

    if (!should_update) {
        return false;
    }

    SearchResult enriched = candidate;
    enriched.principal_variation = extract_principal_variation(
        board, tt, tt_generation, std::max(candidate.depth_reached, 1));
    if (enriched.principal_variation.empty() && candidate.has_move) {
        enriched.principal_variation.push_back(candidate.best_move);
    }
    if (enriched.seldepth < candidate.seldepth) {
        enriched.seldepth = candidate.seldepth;
    }
    std::uint64_t nodes = shared_state.node_counter.load(std::memory_order_relaxed);
    enriched.nodes = nodes;
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - shared_state.start_time);
    long long elapsed_ms = elapsed.count();
    if (elapsed_ms < 0) {
        elapsed_ms = 0;
    }
    if (elapsed_ms > std::numeric_limits<int>::max()) {
        elapsed_ms = std::numeric_limits<int>::max();
    }
    enriched.time_ms = static_cast<int>(elapsed_ms);

    {
        std::lock_guard lock(shared.mutex);
        shared.result = enriched;
    }

    if (announce_update) {
        announce_search_update(board, enriched, shared_state);
    }

    return true;
}

SearchResult run_search_thread(const Board &board, int max_depth_limit, SearchSharedState &shared,
                               SharedBestResult &shared_result, const SearchResult &seed,
                               int thread_index, bool is_primary, GlobalTranspositionTable &tt,
                               std::uint8_t tt_generation) {
    SearchContext context;
    context.shared = &shared;
    context.tt = &tt;
    context.tt_generation = tt_generation;
    SearchResult local = seed;
    Move best_move = seed.has_move ? seed.best_move : Move{};
    bool best_found = seed.has_move;
    int previous_score = seed.score;
    bool have_previous = seed.has_move;

    initialize_evaluation(board);

    if (thread_index > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds{15 * thread_index});
    }

    for (int depth = 1; depth <= max_depth_limit; ++depth) {
        if (shared.stop.load(std::memory_order_relaxed)) {
            break;
        }

        const int full_min = std::numeric_limits<int>::min() / 2;
        const int full_max = std::numeric_limits<int>::max() / 2;
        int aspiration_window = 25;
        int alpha = have_previous ? previous_score - aspiration_window : full_min;
        int beta = have_previous ? previous_score + aspiration_window : full_max;

        Move current_best{};
        bool found = false;
        int score = 0;
        auto iteration_start = std::chrono::steady_clock::now();
        context.selective_depth = 0;

        while (true) {
            score = negamax(board, depth, alpha, beta, 0, &current_best, &found, context, 0, true);
            if (shared.stop.load(std::memory_order_relaxed)) {
                local.timed_out = shared.timed_out.load(std::memory_order_relaxed);
                break;
            }
            if (have_previous && score <= alpha) {
                aspiration_window *= 2;
                alpha = std::max(full_min, previous_score - aspiration_window);
                beta = std::min(full_max, previous_score + aspiration_window);
                continue;
            }
            if (have_previous && score >= beta) {
                aspiration_window *= 2;
                beta = std::min(full_max, previous_score + aspiration_window);
                alpha = std::max(full_min, previous_score - aspiration_window);
                continue;
            }
            break;
        }

        auto iteration_end = std::chrono::steady_clock::now();
        context.last_iteration_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            iteration_end - iteration_start);

        if (shared.stop.load(std::memory_order_relaxed)) {
            break;
        }

        previous_score = score;
        have_previous = true;
        local.score = score;
        local.seldepth = std::max(local.seldepth, context.selective_depth);
        if (found) {
            best_move = current_best;
            best_found = true;
            local.best_move = current_best;
            local.has_move = true;
            local.depth_reached = depth;
            publish_best_result(local, shared_result, board, tt, tt_generation, shared,
                                is_primary);
        }

        if (is_primary && shared.has_time_limit) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                iteration_end - shared.start_time);
            if (elapsed >= shared.hard_time_limit) {
                shared.stop.store(true, std::memory_order_relaxed);
                shared.timed_out.store(true, std::memory_order_relaxed);
                local.timed_out = true;
                break;
            }
            auto projected = context.last_iteration_time * 3 / 2;
            if (elapsed + projected >= shared.soft_time_limit ||
                elapsed >= shared.soft_time_limit) {
                shared.stop.store(true, std::memory_order_relaxed);
                break;
            }
        }
    }

    if (best_found) {
        local.best_move = best_move;
        local.has_move = true;
    }
    publish_best_result(local, shared_result, board, tt, tt_generation, shared, false);

    return local;
}

SearchResult search_best_move(const Board &board, const SearchLimits &limits) {
    SearchResult result;
    int max_depth_limit = limits.max_depth > 0 ? limits.max_depth : max_search_depth;
    max_depth_limit = std::min(max_depth_limit, max_search_depth);

    SearchSharedState shared;
    shared.start_time = std::chrono::steady_clock::now();

    std::optional<TimeAllocation> allocation = compute_time_allocation(board, limits);
    std::uint64_t nodes_budget_from_time = 0;
    if (allocation.has_value()) {
        shared.has_time_limit = true;
        shared.soft_time_limit = allocation->soft;
        shared.hard_time_limit = allocation->hard;
        if (shared.soft_time_limit.count() <= 0) {
            shared.soft_time_limit = std::chrono::milliseconds{1};
        }
        if (shared.hard_time_limit.count() <= 0) {
            shared.hard_time_limit = shared.soft_time_limit;
        }
        shared.start_time = std::chrono::steady_clock::now();
        nodes_budget_from_time = nodes_budget_for_time(shared.hard_time_limit);
    }

    if (limits.max_nodes > 0) {
        shared.has_node_limit = true;
        shared.node_limit = limits.max_nodes;
        if (nodes_budget_from_time > 0) {
            shared.node_limit = std::min(shared.node_limit, nodes_budget_from_time);
        }
    } else if (nodes_budget_from_time > 0) {
        shared.has_node_limit = true;
        shared.node_limit = nodes_budget_from_time;
    }

    int root_piece_count = total_piece_count(board);
    if (syzygy::available() && root_piece_count <= syzygy::probe_piece_limit() &&
        syzygy::max_pieces() >= root_piece_count) {
        if (auto root_probe = syzygy::probe_root(board); root_probe.has_value()) {
            result.score = syzygy_wdl_to_score(root_probe->wdl, 0);
            if (root_probe->best_move.has_value()) {
                result.best_move = *root_probe->best_move;
                result.has_move = true;
                result.depth_reached = 0;
                if (std::abs(result.score) >= mate_threshold || root_probe->wdl == 0) {
                    return result;
                }
            }
        }
    }

    SearchResult seed = result;

    SharedBestResult shared_result;
    shared_result.result = seed;

    GlobalTranspositionTable &tt = shared_transposition_table();
    std::uint8_t tt_generation = tt.prepare_for_search();

    ActiveSearchGuard active_guard{&shared};

    int thread_count = std::max(1, get_search_threads());
    std::vector<std::thread> workers;
    workers.reserve(static_cast<std::size_t>(std::max(0, thread_count - 1)));
    std::vector<SearchResult> thread_results(static_cast<std::size_t>(thread_count));

    auto worker_fn = [&](int index, bool is_primary) {
        thread_results[static_cast<std::size_t>(index)] = run_search_thread(
            board, max_depth_limit, shared, shared_result, seed, index, is_primary, tt, tt_generation);
    };

    for (int index = 1; index < thread_count; ++index) {
        workers.emplace_back(worker_fn, index, false);
    }

    worker_fn(0, true);

    shared.stop.store(true, std::memory_order_relaxed);

    for (auto &thread : workers) {
        thread.join();
    }

    SearchResult best = shared_result.result;
    for (const auto &candidate : thread_results) {
        if (!candidate.has_move) {
            continue;
        }
        if (!best.has_move || candidate.depth_reached > best.depth_reached ||
            (candidate.depth_reached == best.depth_reached &&
             std::abs(candidate.score) >= std::abs(best.score))) {
            best = candidate;
        }
    }

    if (!best.has_move) {
        auto legal_moves = generate_legal_moves(board);
        if (!legal_moves.empty()) {
            best.best_move = legal_moves.front();
            best.has_move = true;
        }
    }

    best.nodes = shared.node_counter.load(std::memory_order_relaxed);
    if (shared_result.result.seldepth > best.seldepth) {
        best.seldepth = shared_result.result.seldepth;
    }
    if (best.has_move && best.principal_variation.empty()) {
        best.principal_variation = extract_principal_variation(
            board, tt, tt_generation, std::max(best.depth_reached, 1));
        if (best.principal_variation.empty()) {
            best.principal_variation.push_back(best.best_move);
        }
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - shared.start_time);
    long long elapsed_ms = elapsed.count();
    if (elapsed_ms < 0) {
        elapsed_ms = 0;
    }
    if (elapsed_ms > std::numeric_limits<int>::max()) {
        elapsed_ms = std::numeric_limits<int>::max();
    }
    best.time_ms = static_cast<int>(elapsed_ms);
    if (shared.timed_out.load(std::memory_order_relaxed)) {
        best.timed_out = true;
    }

    return best;
}

std::string format_uci_score(int score) {
    if (score >= mate_threshold || score <= -mate_threshold) {
        int moves = (mate_score - std::abs(score) + 1) / 2;
        if (score < 0) {
            moves = -moves;
        }
        return std::string{"mate "} + std::to_string(moves);
    }
    return std::string{"cp "} + std::to_string(score);
}

std::string principal_variation_to_uci(const Board &board, const std::vector<Move> &pv) {
    if (pv.empty()) {
        return {};
    }
    Board current = board;
    std::ostringstream stream;
    bool first = true;
    for (const Move &move : pv) {
        auto moves = generate_legal_moves(current);
        auto it = std::find_if(moves.begin(), moves.end(), [&](const Move &candidate) {
            return candidate.from == move.from && candidate.to == move.to &&
                   candidate.piece == move.piece && candidate.captured == move.captured &&
                   candidate.promotion == move.promotion &&
                   candidate.is_en_passant == move.is_en_passant &&
                   candidate.is_castling == move.is_castling;
        });
        if (it == moves.end()) {
            break;
        }
        if (!first) {
            stream << ' ';
        }
        stream << move_to_uci(*it);
        current = current.apply_move(*it);
        first = false;
    }
    return stream.str();
}

bool save_transposition_table(const std::string &path, std::string *error) {
    return shared_transposition_table().save(path, error);
}

bool load_transposition_table(const std::string &path, std::string *error) {
    return shared_transposition_table().load(path, error);
}

void request_stop_search() {
    std::lock_guard<std::mutex> lock(active_search_mutex);
    if (active_search_state != nullptr) {
        active_search_state->stop.store(true, std::memory_order_relaxed);
    }
}

}  // namespace sirio
