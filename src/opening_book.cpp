#include "sirio/opening_book.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "sirio/move.hpp"
#include "sirio/movegen.hpp"

namespace sirio::book {

namespace {

struct BookEntry {
    std::string move;
    std::uint32_t weight = 1;
};

std::unordered_map<std::string, std::vector<BookEntry>> g_entries;
std::mutex g_mutex;
bool g_loaded = false;
std::mt19937 &rng() {
    static std::mt19937 instance{std::random_device{}()};
    return instance;
}

std::string trim(std::string_view view) {
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

std::string normalize_fen_key(const std::string &fen) {
    std::istringstream stream{fen};
    std::string fields[4];
    for (int i = 0; i < 4; ++i) {
        if (!(stream >> fields[i])) {
            return {};
        }
    }
    return fields[0] + " " + fields[1] + " " + fields[2] + " " + fields[3];
}

}  // namespace

bool load(const std::string &path, std::string *error) {
    std::ifstream file(path);
    if (!file) {
        if (error != nullptr) {
            *error = "No se pudo abrir el libro de aperturas: " + path;
        }
        return false;
    }

    std::unordered_map<std::string, std::vector<BookEntry>> new_entries;
    std::string line;
    std::size_t line_number = 0;
    std::size_t loaded_entries = 0;
    std::vector<std::string> issues;

    while (std::getline(file, line)) {
        ++line_number;
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == '#') {
            continue;
        }
        auto first_sep = trimmed.find(';');
        if (first_sep == std::string::npos) {
            issues.push_back("Línea " + std::to_string(line_number) + ": formato inválido");
            continue;
        }
        auto second_sep = trimmed.find(';', first_sep + 1);
        std::string fen = trim(trimmed.substr(0, first_sep));
        std::string move = trim(trimmed.substr(first_sep + 1, second_sep == std::string::npos
                                                             ? std::string::npos
                                                             : second_sep - first_sep - 1));
        std::string weight_text = second_sep == std::string::npos
                                      ? std::string{}
                                      : trim(trimmed.substr(second_sep + 1));
        if (fen.empty() || move.empty()) {
            issues.push_back("Línea " + std::to_string(line_number) + ": FEN o movimiento vacío");
            continue;
        }
        std::string key = normalize_fen_key(fen);
        if (key.empty()) {
            issues.push_back("Línea " + std::to_string(line_number) + ": FEN inválido");
            continue;
        }
        std::uint32_t weight = 1;
        if (!weight_text.empty()) {
            try {
                unsigned long parsed = std::stoul(weight_text);
                if (parsed > 0) {
                    weight = static_cast<std::uint32_t>(parsed);
                }
            } catch (const std::exception &) {
                issues.push_back("Línea " + std::to_string(line_number) + ": peso inválido");
                continue;
            }
        }
        new_entries[key].push_back(BookEntry{move, weight});
        ++loaded_entries;
    }

    if (loaded_entries == 0) {
        if (error != nullptr) {
            *error = "El libro no contiene movimientos válidos";
            if (!issues.empty()) {
                *error += ". Primer problema: " + issues.front();
            }
        }
        return false;
    }

    if (error != nullptr && !issues.empty()) {
        *error = issues.front();
    }

    std::lock_guard lock(g_mutex);
    g_entries = std::move(new_entries);
    g_loaded = true;
    return true;
}

void clear() {
    std::lock_guard lock(g_mutex);
    g_entries.clear();
    g_loaded = false;
}

bool is_loaded() {
    std::lock_guard lock(g_mutex);
    return g_loaded;
}

std::optional<Move> choose_move(const Board &board) {
    std::string key = normalize_fen_key(board.to_fen());
    std::string selected_move;
    {
        std::lock_guard lock(g_mutex);
        if (!g_loaded) {
            return std::nullopt;
        }
        auto it = g_entries.find(key);
        if (it == g_entries.end()) {
            return std::nullopt;
        }
        const auto &moves = it->second;
        std::uint64_t total_weight = 0;
        for (const auto &entry : moves) {
            total_weight += entry.weight;
        }
        if (total_weight == 0) {
            return std::nullopt;
        }
        std::uniform_int_distribution<std::uint64_t> dist(1, total_weight);
        std::uint64_t target = dist(rng());
        for (const auto &entry : moves) {
            if (target <= entry.weight) {
                selected_move = entry.move;
                break;
            }
            target -= entry.weight;
        }
    }

    if (selected_move.empty()) {
        return std::nullopt;
    }

    try {
        return move_from_uci(board, selected_move);
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

}  // namespace sirio::book

