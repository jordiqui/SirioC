#include <cstdint>
#include <iostream>
#include <string>

#include "sirio/evaluation.hpp"
#include "sirio/evaluation_route.hpp"

namespace {

struct FenCase {
    const char *fen;
    const char *name;
};

constexpr FenCase kFens[] = {
    {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "startpos"},
    {"8/8/8/8/8/8/6k1/6K1 w - - 0 1", "kings_only"},
    {"4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1", "asymmetric_material"},
};

int run(const std::string &network_path) {
    sirio::ExperimentalSirioNNUE2Runtime runtime{};
    const auto init = sirio::initialize_sirio_nnue2_shadow_runtime_for_tests(network_path, runtime);
    if (!init.load_succeeded) {
        std::cout << "LOAD_REJECTED|" << init.fallback_reason << "\n";
        return 2;
    }

    sirio::InternalEvalBackendSelection default_selection{};
    sirio::InternalEvalBackendSelection experimental_selection{};
    experimental_selection.backend = sirio::InternalEvalBackend::ExperimentalSirioNNUE2;

    for (const auto &fen_case : kFens) {
        sirio::Board board{fen_case.fen};
        const auto before = board.to_fen();
        sirio::initialize_evaluation(board);
        const auto baseline = sirio::evaluate(board);

        const auto default_out = sirio::evaluate_with_experimental_selector_shadow_for_tests(
            board, default_selection, runtime);
        const auto experimental_out = sirio::evaluate_with_experimental_selector_shadow_for_tests(
            board, experimental_selection, runtime);
        const auto repeated_out = sirio::evaluate_with_experimental_selector_shadow_for_tests(
            board, experimental_selection, runtime);

        const bool deterministic = repeated_out.score == experimental_out.score;
        const bool board_preserved = before == board.to_fen();

        std::cout << "CASE|" << fen_case.name << "|baseline=" << baseline
                  << "|default_score=" << default_out.score
                  << "|default_backend=" << static_cast<int>(default_out.actual_backend)
                  << "|experimental_score=" << experimental_out.score
                  << "|experimental_backend=" << static_cast<int>(experimental_out.actual_backend)
                  << "|experimental_fallback=" << (experimental_out.fallback_occurred ? 1 : 0)
                  << "|deterministic=" << (deterministic ? 1 : 0)
                  << "|board_preserved=" << (board_preserved ? 1 : 0)
                  << "\n";

        if (default_out.score != baseline ||
            default_out.actual_backend != sirio::InternalEvalBackend::DefaultExisting ||
            experimental_out.actual_backend != sirio::InternalEvalBackend::ExperimentalSirioNNUE2 ||
            experimental_out.fallback_occurred || !deterministic || !board_preserved) {
            return 3;
        }
    }

    std::cout << "ACTIVATION_OK\n";
    return 0;
}

}  // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "usage: sirio_nnue_internal_activation_contract <verified_candidate.nnue2>\n";
        return 1;
    }
    return run(argv[1]);
}
