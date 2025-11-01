#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "sirio/board.hpp"
#include "sirio/evaluation.hpp"
#include "sirio/move.hpp"
#include "sirio/search.hpp"
#include "sirio/syzygy.hpp"
#include "sirio/nnue/backend.hpp"

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

    sirio::use_classical_evaluation();

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

    struct EvaluationSample {
        std::string label;
        std::string fen;
    };

    std::vector<EvaluationSample> evaluation_samples = {
        {"Midgame passed pawn", "r3k2r/ppp2ppp/8/8/3P4/8/PPP2PPP/R3K2R w KQkq - 0 1"},
        {"Endgame passed pawn", "6k1/8/4P3/8/3K4/8/8/8 w - - 0 1"}};

    std::cout << "Evaluation sample (phase-aware):\n";
    for (const auto &entry : evaluation_samples) {
        sirio::Board board{entry.fen};
        sirio::initialize_evaluation(board);
        int score = sirio::evaluate(board);
        std::cout << "  " << entry.label << ": " << score << " (" << entry.fen << ")\n";
    }
    std::cout << "\n";

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

    // NNUE evaluation throughput benchmark
    std::filesystem::path default_network = std::filesystem::path(__FILE__).parent_path() / "../tests/data/minimal.nnue";
    const char *override_path = std::getenv("SIRIO_NNUE_BENCH");
    std::filesystem::path nnue_path = override_path ? std::filesystem::path(override_path) : default_network;
    std::error_code nnue_ec;
    auto canonical = std::filesystem::weakly_canonical(nnue_path, nnue_ec);
    if (!nnue_ec) {
        nnue_path = canonical;
    }

    sirio::nnue::SingleNetworkBackend nnue_backend;
    std::string nnue_error;
    if (nnue_backend.load(nnue_path.string(), &nnue_error)) {
        std::vector<std::string> eval_positions = {
            "rnbqk2r/ppp2ppp/5n2/3pp3/3P4/2P1PN2/PP1NBPPP/R2QKB1R w KQ - 4 8",
            "4rrk1/pp1n1ppp/2p2q2/3p4/3P1B2/2NQ1N2/PP3PPP/4RRK1 w - - 0 1",
            "2r2rk1/pp2qpp1/2p4p/3pP3/3P1P2/2N3Q1/PP4PP/2RR2K1 w - - 2 20",
            "r3k2r/ppp2ppp/8/8/3P4/8/PPP2PPP/R3K2R w KQkq - 0 1",
            "6k1/5ppp/8/6Q1/8/8/8/6K1 w - - 0 1",
            "8/6pp/3bp3/3p1p2/3P1P2/3BP3/6PP/6K1 w - - 0 1",
            "4r1k1/pp3pbp/2p3p1/3n4/3P1B2/2N4P/PP3PP1/4R1K1 w - - 0 21",
            "3rr1k1/pp2qppp/2p1bn2/3p4/3P1B2/2N1PN2/PPQ2PPP/3RR1K1 w - - 9 18",
            "2r3k1/1p2qpp1/p1n4p/3p4/3P1B2/2N2Q1P/PP3PP1/2RR2K1 w - - 0 23",
            "8/8/8/8/8/8/6k1/6K1 w - - 0 1"
        };

        std::vector<sirio::nnue::FeatureState> states;
        states.reserve(eval_positions.size());
        for (const auto &fen : eval_positions) {
            sirio::Board board{fen};
            states.push_back(nnue_backend.extract_features(board));
        }

        std::vector<int> outputs(states.size());
        constexpr std::size_t iterations = 200000;
        std::int64_t checksum = 0;
        auto nnue_start = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < iterations; ++i) {
            nnue_backend.evaluate_batch(states, outputs);
            checksum = std::accumulate(outputs.begin(), outputs.end(), checksum);
        }
        auto nnue_end = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(nnue_end - nnue_start);
        double total_positions = static_cast<double>(iterations) * static_cast<double>(states.size());
        double seconds_elapsed = static_cast<double>(elapsed.count()) / 1'000'000'000.0;
        double throughput = seconds_elapsed > 0.0 ? total_positions / seconds_elapsed : 0.0;
        double avg_latency_us = total_positions > 0.0 ? (static_cast<double>(elapsed.count()) / total_positions) / 1000.0 : 0.0;

        std::cout << "NNUE evaluation benchmark:\n";
        std::cout << "  Network: " << nnue_path << "\n";
        std::cout << "  Batch size: " << states.size() << "\n";
        std::cout << "  Iterations: " << iterations << "\n";
        std::cout << "  Total evaluations: " << static_cast<std::uint64_t>(total_positions) << "\n";
        std::cout << "  Throughput (evals/s): " << static_cast<std::uint64_t>(throughput) << "\n";
        std::cout << "  Average latency (us): " << avg_latency_us << "\n";
        std::cout << "  Checksum: " << checksum << "\n\n";
    } else {
        std::cout << "NNUE evaluation benchmark skipped: " << nnue_error << " (expected at " << nnue_path << ")\n\n";
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

