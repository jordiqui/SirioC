#include "files/pgn_loader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace sirio::files {

namespace {

std::string sanitize_moves(const std::string& moves) {
    std::string sanitized;
    sanitized.reserve(moves.size());

    int variation_depth = 0;
    bool in_comment = false;

    for (char ch : moves) {
        if (ch == '{') {
            in_comment = true;
            continue;
        }
        if (ch == '}') {
            in_comment = false;
            continue;
        }
        if (in_comment) {
            continue;
        }
        if (ch == '(') {
            ++variation_depth;
            continue;
        }
        if (ch == ')') {
            if (variation_depth > 0) {
                --variation_depth;
            }
            continue;
        }
        if (variation_depth > 0) {
            continue;
        }
        sanitized.push_back(ch);
    }

    return sanitized;
}

void parse_tag(const std::string& line, PgnGame& game) {
    const auto first_space = line.find(' ');
    if (first_space == std::string::npos) {
        return;
    }

    const std::string key = line.substr(1, first_space - 1);
    const auto first_quote = line.find('"', first_space);
    const auto last_quote = line.rfind('"');

    if (first_quote == std::string::npos || last_quote == std::string::npos || last_quote <= first_quote) {
        return;
    }

    const std::string value = line.substr(first_quote + 1, last_quote - first_quote - 1);
    game.tags[key] = value;
}

bool is_result_token(const std::string& token) {
    return token == "1-0" || token == "0-1" || token == "1/2-1/2" || token == "*";
}

}  // namespace

PgnGame load_pgn(const std::string& content) {
    PgnGame game;

    std::istringstream lines(content);
    std::string line;
    std::ostringstream move_buffer;

    while (std::getline(lines, line)) {
        if (line.empty()) {
            continue;
        }

        if (line.front() == '[') {
            parse_tag(line, game);
        } else {
            if (!move_buffer.str().empty()) {
                move_buffer << ' ';
            }
            move_buffer << line;
        }
    }

    const std::string sanitized = sanitize_moves(move_buffer.str());
    std::istringstream tokens(sanitized);
    std::string token;

    while (tokens >> token) {
        const auto dot_pos = token.find('.');
        if (dot_pos != std::string::npos) {
            // Remove move number prefixes like "12." or "12..."
            const auto rest = token.substr(dot_pos + 1);
            if (!rest.empty()) {
                token = rest;
            } else {
                continue;
            }
        }

        if (token.empty() || is_result_token(token)) {
            if (is_result_token(token)) {
                game.result = token;
            }
            continue;
        }

        game.moves.push_back(token);
    }

    if (game.result.empty()) {
        game.result = "*";
    }

    return game;
}

PgnGame load_pgn_from_file(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("Unable to open PGN file: " + path);
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return load_pgn(buffer.str());
}

}  // namespace sirio::files
