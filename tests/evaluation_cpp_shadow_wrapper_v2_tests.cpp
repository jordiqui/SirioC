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
    const auto out = root / "build" / "test_eval_cpp_shadow_wrapper_v2.bin";
    const auto script = root / "training" / "nnue" / "scripts" / "export_to_engine_v2.py";
    const std::string cmd = "python " + script.string() + " --output " + out.string();
    assert(std::system(cmd.c_str()) == 0);
    return out;
}

void test_evaluate_default_path_is_unchanged_for_reference_fens() {
    const std::string fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "8/8/8/8/8/8/6k1/6K1 w - - 0 1",
        "4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1",
    };

    for (const auto &fen : fens) {
        sirio::Board board{fen};
        sirio::initialize_evaluation(board);
        const auto first = sirio::evaluate(board);
        const auto second = sirio::evaluate(board);
        assert(first == second);
    }
}

void test_wrapper_default_existing_matches_evaluate_and_preserves_board() {
    sirio::ExperimentalSirioNNUE2Runtime runtime{};
    const sirio::InternalEvalBackendSelection selection{};

    const std::string fen = "4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1";
    sirio::Board board{fen};
    const auto before_fen = board.to_fen();
    sirio::initialize_evaluation(board);
    const auto expected = sirio::evaluate(board);

    const auto wrapped =
        sirio::evaluate_with_experimental_selector_shadow_for_tests(board, selection, runtime);
    assert(wrapped.score == expected);
    assert(wrapped.requested_backend == sirio::InternalEvalBackend::DefaultExisting);
    assert(wrapped.actual_backend == sirio::InternalEvalBackend::DefaultExisting);
    assert(!wrapped.fallback_occurred);
    assert(board.to_fen() == before_fen);
}

void test_wrapper_experimental_inactive_runtime_falls_back_with_metadata() {
    sirio::ExperimentalSirioNNUE2Runtime runtime{};
    sirio::InternalEvalBackendSelection selection{};
    selection.backend = sirio::InternalEvalBackend::ExperimentalSirioNNUE2;

    const std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    sirio::Board board{fen};
    sirio::initialize_evaluation(board);
    const auto expected = sirio::evaluate(board);

    const auto wrapped =
        sirio::evaluate_with_experimental_selector_shadow_for_tests(board, selection, runtime);
    assert(wrapped.score == expected);
    assert(wrapped.requested_backend == sirio::InternalEvalBackend::ExperimentalSirioNNUE2);
    assert(wrapped.actual_backend == sirio::InternalEvalBackend::DefaultExisting);
    assert(wrapped.fallback_occurred);
    assert(wrapped.fallback_status ==
           sirio::InternalEvalBackendFallbackStatus::RuntimeInactiveOrUnloaded);
    assert(wrapped.consulted_experimental_runtime);
}

void test_wrapper_loaded_runtime_matches_selector_helper_is_deterministic_and_preserves_board() {
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

        const auto wrapped =
            sirio::evaluate_with_experimental_selector_shadow_for_tests(board, selection, runtime);
        const auto selector =
            sirio::evaluate_with_internal_eval_selector_for_tests(board, selection, runtime);
        assert(wrapped.score == selector.score);
        assert(wrapped.actual_backend == selector.actual_backend);
        assert(wrapped.fallback_occurred == selector.fallback_occurred);

        const auto repeat =
            sirio::evaluate_with_experimental_selector_shadow_for_tests(board, selection, runtime);
        assert(repeat.score == wrapped.score);
        assert(board.to_fen() == before_fen);
    }
}

}  // namespace

void run_evaluation_cpp_shadow_wrapper_v2_tests() {
    test_evaluate_default_path_is_unchanged_for_reference_fens();
    test_wrapper_default_existing_matches_evaluate_and_preserves_board();
    test_wrapper_experimental_inactive_runtime_falls_back_with_metadata();
    test_wrapper_loaded_runtime_matches_selector_helper_is_deterministic_and_preserves_board();
}
