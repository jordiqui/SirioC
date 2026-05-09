#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "sirio/evaluation.hpp"
#include "sirio/evaluation_route.hpp"

namespace {

std::filesystem::path repo_root() { return std::filesystem::path(__FILE__).parent_path().parent_path(); }

std::filesystem::path build_fixture_network() {
    const auto root = repo_root();
    const auto out = root / "build" / "test_eval_internal_selector_v2.bin";
    const auto script = root / "training" / "nnue" / "scripts" / "export_to_engine_v2.py";
    const std::string cmd = "python " + script.string() + " --output " + out.string();
    assert(std::system(cmd.c_str()) == 0);
    return out;
}

void test_selector_helper_preserves_default_and_board_for_fixed_fens() {
    const std::string fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "8/8/8/8/8/8/6k1/6K1 w - - 0 1",
        "4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1",
    };

    sirio::ExperimentalSirioNNUE2Runtime runtime{};
    const sirio::InternalEvalBackendSelection default_selection{};

    for (const auto &fen : fens) {
        sirio::Board board{fen};
        const auto before_fen = board.to_fen();
        sirio::initialize_evaluation(board);
        const auto base_score = sirio::evaluate(board);
        const auto after_base_score = sirio::evaluate(board);
        assert(base_score == after_base_score);

        const auto selected = sirio::evaluate_with_internal_eval_selector_for_tests(
            board, default_selection, runtime);
        assert(selected.score == base_score);
        assert(selected.requested_backend == sirio::InternalEvalBackend::DefaultExisting);
        assert(selected.actual_backend == sirio::InternalEvalBackend::DefaultExisting);
        assert(!selected.fallback_occurred);
        assert(board.to_fen() == before_fen);
    }
}

void test_selector_helper_experimental_inactive_runtime_fallback_contract() {
    sirio::ExperimentalSirioNNUE2Runtime runtime{};
    sirio::InternalEvalBackendSelection selection{};
    selection.backend = sirio::InternalEvalBackend::ExperimentalSirioNNUE2;

    const std::string fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "8/8/8/8/8/8/6k1/6K1 w - - 0 1",
        "4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1",
    };

    for (const auto &fen : fens) {
        sirio::Board board{fen};
        sirio::initialize_evaluation(board);
        const auto default_score = sirio::evaluate(board);
        const auto result =
            sirio::evaluate_with_internal_eval_selector_for_tests(board, selection, runtime);
        assert(result.score == default_score);
        assert(result.fallback_occurred);
        assert(result.fallback_status ==
               sirio::InternalEvalBackendFallbackStatus::RuntimeInactiveOrUnloaded);
    }
}

void test_selector_helper_loaded_runtime_matches_backend_selector_helper_and_is_deterministic() {
    const auto network_path = build_fixture_network();
    sirio::ExperimentalSirioNNUE2Runtime runtime{};
    assert(runtime.load_from_file(network_path.string()));

    sirio::InternalEvalBackendSelection selection{};
    selection.backend = sirio::InternalEvalBackend::ExperimentalSirioNNUE2;

    const std::string fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "8/8/8/8/8/8/6k1/6K1 w - - 0 1",
        "4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1",
    };

    for (const auto &fen : fens) {
        sirio::Board board{fen};
        const auto before_fen = board.to_fen();
        sirio::initialize_evaluation(board);
        const auto default_score = sirio::evaluate(board);

        const auto via_layer =
            sirio::evaluate_with_internal_eval_selector_for_tests(board, selection, runtime);
        const auto via_backend = sirio::evaluate_with_internal_eval_backend_for_tests(
            board, default_score, selection, runtime);
        assert(via_layer.score == via_backend.score);
        assert(via_layer.actual_backend == via_backend.actual_backend);
        assert(via_layer.fallback_occurred == via_backend.fallback_occurred);

        const auto repeat =
            sirio::evaluate_with_internal_eval_selector_for_tests(board, selection, runtime);
        assert(repeat.score == via_layer.score);
        assert(board.to_fen() == before_fen);
    }
}

}  // namespace

void run_evaluation_internal_selector_v2_tests() {
    test_selector_helper_preserves_default_and_board_for_fixed_fens();
    test_selector_helper_experimental_inactive_runtime_fallback_contract();
    test_selector_helper_loaded_runtime_matches_backend_selector_helper_and_is_deterministic();
}
