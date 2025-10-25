#include <cctype>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "sirio/board.hpp"
#include "sirio/move.hpp"
#include "sirio/movegen.hpp"
#include "sirio/search.hpp"

namespace {

void send_uci_id() {
    std::cout << "id name SirioC" << std::endl;
    std::cout << "id author OpenAI" << std::endl;
    std::cout << "uciok" << std::endl;
}

void send_ready() { std::cout << "readyok" << std::endl; }

std::string trim_leading(std::string_view view) {
    std::size_t pos = 0;
    while (pos < view.size() && std::isspace(static_cast<unsigned char>(view[pos]))) {
        ++pos;
    }
    return std::string{view.substr(pos)};
}

void set_position(sirio::Board &board, const std::string &command_args) {
    std::istringstream stream{command_args};
    std::string token;
    if (!(stream >> token)) {
        return;
    }

    if (token == "startpos") {
        board = sirio::Board{};
        if (!(stream >> token)) {
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
            return;
        }
    } else {
        throw std::runtime_error("Unsupported position command");
    }

    if (token != "moves") {
        // Skip until we find moves keyword or end
        while (stream >> token) {
            if (token == "moves") {
                break;
            }
        }
    }

    if (token != "moves") {
        return;
    }

    while (stream >> token) {
        try {
            sirio::Move move = sirio::move_from_uci(board, token);
            board = board.apply_move(move);
        } catch (const std::exception &) {
            // Ignore illegal moves in history to maintain robustness
            break;
        }
    }
}

void handle_go(const std::string &command_args, const sirio::Board &board) {
    std::istringstream stream{command_args};
    std::string token;
    int depth = 4;
    while (stream >> token) {
        if (token == "depth") {
            if (stream >> token) {
                depth = std::stoi(token);
            }
        }
    }

    sirio::SearchResult result = sirio::search_best_move(board, depth);
    if (result.has_move) {
        std::cout << "info depth " << depth << " score cp " << result.score << " pv "
                  << sirio::move_to_uci(result.best_move) << std::endl;
        std::cout << "bestmove " << sirio::move_to_uci(result.best_move) << std::endl;
    } else {
        std::cout << "bestmove 0000" << std::endl;
    }
}

}  // namespace

int main() {
    sirio::Board board;
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
                send_ready();
            } else if (command == "ucinewgame") {
                board = sirio::Board{};
            } else if (command == "position") {
                std::string rest;
                std::getline(stream, rest);
                set_position(board, rest);
            } else if (command == "go") {
                std::string rest;
                std::getline(stream, rest);
                handle_go(rest, board);
            } else if (command == "quit" || command == "stop") {
                break;
            } else if (command == "d") {
                std::cout << board.to_fen() << std::endl;
            }
        } catch (const std::exception &ex) {
            std::cerr << "Error: " << ex.what() << std::endl;
        }
    }

    return 0;
}

