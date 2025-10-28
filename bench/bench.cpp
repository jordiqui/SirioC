#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "sirio/board.hpp"
#include "sirio/move.hpp"
#include "sirio/search.hpp"
#include "sirio/syzygy.hpp"

namespace {
struct TacticalPosition {
    std::string fen;
    std::string best_move;
};
}

int main() {
    if (auto detected = sirio::syzygy::detect_default_tablebase_path(); detected.has_value()) {
        sirio::syzygy::set_tablebase_path(detected->string());
    }

    std::vector<std::string> speed_positions = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r1bq1rk1/ppp2ppp/2n2n2/3pp3/3P4/2P1PN2/PP1NBPPP/R2QKB1R w KQ - 0 7",
        "3r2k1/pp3ppp/2n1b3/3p4/3P4/2P1BN2/PP3PPP/3R2K1 w - - 0 1"};

    sirio::SearchLimits speed_limits;
    speed_limits.max_depth = 4;

    std::uint64_t total_nodes = 0;
    auto speed_start = std::chrono::steady_clock::now();
    for (const auto &fen : speed_positions) {
        sirio::Board board{fen};
        auto result = sirio::search_best_move(board, speed_limits);
        total_nodes += result.nodes;
    }
    auto speed_end = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(speed_end - speed_start);
    double seconds = static_cast<double>(elapsed_ms.count()) / 1000.0;
    double nps = seconds > 0.0 ? static_cast<double>(total_nodes) / seconds : 0.0;

    std::cout << "Search speed benchmark:\n";
    std::cout << "  Positions: " << speed_positions.size() << "\n";
    std::cout << "  Time: " << elapsed_ms.count() << " ms\n";
    std::cout << "  Nodes: " << total_nodes << "\n";
    std::cout << "  Nodes per second: " << static_cast<std::uint64_t>(nps) << "\n\n";

    std::vector<TacticalPosition> tactical_suite = {
        {"6k1/5ppp/8/6Q1/8/8/8/6K1 w - - 0 1", "g5d8"},
        {"k7/8/8/8/8/8/5PPP/6KQ w - - 0 1", "g2g4"}
    };

    sirio::SearchLimits tactic_limits;
    tactic_limits.max_depth = 1;
    tactic_limits.move_time = 1000;

    int correct = 0;
    std::vector<std::string> mismatch_logs;
    for (const auto &entry : tactical_suite) {
        sirio::Board board{entry.fen};
        auto result = sirio::search_best_move(board, tactic_limits);
        std::string uci = result.has_move ? sirio::move_to_uci(result.best_move) : "(none)";
        if (result.has_move && uci == entry.best_move) {
            ++correct;
        } else {
            mismatch_logs.push_back("  " + entry.fen + " -> esperado " + entry.best_move + ", obtenido " + uci);
        }
    }

    std::cout << "Tactical suite accuracy: " << correct << "/" << tactical_suite.size() << "\n";
    for (const auto &log : mismatch_logs) {
        std::cout << log << "\n";
    }

    const std::string &tb_path = sirio::syzygy::tablebase_path();
    sirio::Board tb_board{"8/8/8/8/8/6k1/6P1/6K1 w - - 0 1"};

    auto probe = sirio::syzygy::probe_root(tb_board);
    if (probe.has_value() && probe->best_move) {
        std::cout << "Syzygy probe move: " << sirio::move_to_uci(*probe->best_move)
                  << " (wdl=" << probe->wdl << ", dtz=" << probe->dtz << ")\n";
    } else {
        std::string reason;
        if (tb_path.empty()) {
            reason = "no se ha detectado ninguna ruta";
        } else if (!sirio::syzygy::available()) {
            reason = "la ruta '" + tb_path + "' no contiene tablebases válidas";
        } else {
            reason = "no hay datos disponibles para la posición de prueba";
        }

        std::cout << "Syzygy tablebases no disponibles (" << reason
                  << "). Ejecuto una búsqueda auxiliar...\n";

        sirio::SearchLimits fallback_limits;
        fallback_limits.max_depth = 18;
        fallback_limits.move_time = 1000;

        auto fallback = sirio::search_best_move(tb_board, fallback_limits);
        if (fallback.has_move) {
            std::cout << "  Fallback best move: " << sirio::move_to_uci(fallback.best_move)
                      << " (score=" << sirio::format_uci_score(fallback.score)
                      << ", depth=" << fallback.depth_reached << ", nodes=" << fallback.nodes << ")\n";
        } else {
            std::cout << "  No se pudo determinar una jugada con la búsqueda auxiliar." << std::endl;
        }

        std::cout << "  Copie los archivos Syzygy (.rtbw/.rtbz) en 'tablebases/' o configure"
                     " la opción UCI SyzygyPath para habilitar la prueba automática.\n";
    }

    return 0;
}

