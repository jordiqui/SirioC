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
    const auto out = root / "build" / "test_internal_eval_backend_selector_minimal_v1.bin";
    const auto script = root / "training" / "nnue" / "scripts" / "export_to_engine_v2.py";
    const std::string cmd = "python " + script.string() + " --output " + out.string();
    assert(std::system(cmd.c_str()) == 0);
    return out;
}

void test_default_selection_contract() {
    sirio::InternalEvalBackendSelection selection{};
    assert(selection.backend == sirio::InternalEvalBackend::DefaultExisting);
}

void test_default_existing_returns_default_and_does_not_consult_runtime() {
    const std::string fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "8/8/8/8/8/8/6k1/6K1 w - - 0 1",
        "4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1",
    };

    sirio::ExperimentalSirioNNUE2Runtime runtime{};
    for (const auto &fen : fens) {
        sirio::Board board{fen};
        const auto board_before = board.to_fen();
        sirio::initialize_evaluation(board);
        const auto default_score = sirio::evaluate(board);

        const auto result = sirio::evaluate_with_internal_eval_backend_for_tests(
            board, default_score, sirio::InternalEvalBackendSelection{}, runtime);
        assert(result.score == default_score);
        assert(result.requested_backend == sirio::InternalEvalBackend::DefaultExisting);
        assert(result.actual_backend == sirio::InternalEvalBackend::DefaultExisting);
        assert(!result.fallback_occurred);
        assert(result.fallback_status == sirio::InternalEvalBackendFallbackStatus::NotNeeded);
        assert(!result.consulted_experimental_runtime);
        assert(!result.experimental_runtime_returned_valid_score);
        assert(board.to_fen() == board_before);
    }
}

void test_experimental_selected_inactive_runtime_falls_back() {
    sirio::ExperimentalSirioNNUE2Runtime runtime{};
    sirio::Board board{"8/8/8/8/8/8/6k1/6K1 w - - 0 1"};

    constexpr std::int32_t default_score = 77;
    sirio::InternalEvalBackendSelection selection{};
    selection.backend = sirio::InternalEvalBackend::ExperimentalSirioNNUE2;
    const auto result = sirio::evaluate_with_internal_eval_backend_for_tests(board, default_score,
                                                                              selection, runtime);
    assert(result.score == default_score);
    assert(result.requested_backend == sirio::InternalEvalBackend::ExperimentalSirioNNUE2);
    assert(result.actual_backend == sirio::InternalEvalBackend::DefaultExisting);
    assert(result.fallback_occurred);
    assert(result.fallback_status == sirio::InternalEvalBackendFallbackStatus::RuntimeInactiveOrUnloaded);
    assert(result.consulted_experimental_runtime);
    assert(!result.experimental_runtime_returned_valid_score);
}

void test_experimental_selected_loaded_runtime_matches_shadow_integration_and_is_deterministic() {
    const auto network_path = build_fixture_network();
    sirio::ExperimentalSirioNNUE2Runtime runtime{};
    assert(runtime.load_from_file(network_path.string()));

    const std::string fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "8/8/8/8/8/8/6k1/6K1 w - - 0 1",
        "4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1",
    };

    sirio::InternalEvalBackendSelection selection{};
    selection.backend = sirio::InternalEvalBackend::ExperimentalSirioNNUE2;

    for (const auto &fen : fens) {
        sirio::Board board{fen};
        const auto before = board.to_fen();
        constexpr std::int32_t default_score = 39;
        const auto shadow = sirio::evaluate_with_sirio_nnue2_shadow_integration_for_tests(
            board, default_score, runtime);
        const auto selected = sirio::evaluate_with_internal_eval_backend_for_tests(
            board, default_score, selection, runtime);

        assert(selected.score == shadow.score);
        assert(selected.requested_backend == sirio::InternalEvalBackend::ExperimentalSirioNNUE2);
        assert(selected.actual_backend == sirio::InternalEvalBackend::ExperimentalSirioNNUE2);
        assert(!selected.fallback_occurred);
        assert(selected.consulted_experimental_runtime);
        assert(selected.experimental_runtime_returned_valid_score);

        const auto repeat = sirio::evaluate_with_internal_eval_backend_for_tests(
            board, default_score, selection, runtime);
        assert(repeat.score == selected.score);
        assert(board.to_fen() == before);
    }
}

void test_normal_evaluate_output_is_unchanged_for_fixed_fens() {
    const std::string fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "8/8/8/8/8/8/6k1/6K1 w - - 0 1",
        "4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1",
    };

    const auto network_path = build_fixture_network();
    sirio::ExperimentalSirioNNUE2Runtime runtime{};
    assert(runtime.load_from_file(network_path.string()));

    sirio::InternalEvalBackendSelection selection{};
    selection.backend = sirio::InternalEvalBackend::ExperimentalSirioNNUE2;

    for (const auto &fen : fens) {
        sirio::Board board{fen};
        sirio::initialize_evaluation(board);
        const auto before = sirio::evaluate(board);
        (void)sirio::evaluate_with_internal_eval_backend_for_tests(board, before, selection, runtime);
        const auto after = sirio::evaluate(board);
        assert(before == after);
    }
}

}  // namespace

void run_internal_eval_backend_selector_v2_tests() {
    test_default_selection_contract();
    test_default_existing_returns_default_and_does_not_consult_runtime();
    test_experimental_selected_inactive_runtime_falls_back();
    test_experimental_selected_loaded_runtime_matches_shadow_integration_and_is_deterministic();
    test_normal_evaluate_output_is_unchanged_for_fixed_fens();
}
