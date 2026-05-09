#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "sirio/evaluation.hpp"
#include "sirio/evaluation_route.hpp"

namespace {

std::filesystem::path repo_root() { return std::filesystem::path(__FILE__).parent_path().parent_path(); }

std::filesystem::path build_fixture_network() {
    const auto root = repo_root();
    const auto out = root / "build" / "test_eval_initialization_shadow_v2.bin";
    const auto script = root / "training" / "nnue" / "scripts" / "export_to_engine_v2.py";
    const std::string cmd = "python " + script.string() + " --output " + out.string();
    assert(std::system(cmd.c_str()) == 0);
    return out;
}

void test_default_route_config_does_not_load_runtime() {
    sirio::ExperimentalSirioNNUE2Runtime runtime{};
    sirio::ExperimentalEvaluationConfig config{};
    config.selected_route = sirio::EvaluationRoute::DefaultExisting;

    const auto init = sirio::initialize_sirio_nnue2_shadow_runtime_for_tests(config, runtime);
    assert(!init.load_requested);
    assert(!init.load_attempted);
    assert(!init.load_succeeded);
    assert(init.runtime_status == sirio::ExperimentalSirioNNUE2RuntimeStatus::Inactive);
    assert(!runtime.is_loaded());
}

void test_explicit_network_path_load_and_shadow_wrapper_evaluation() {
    const auto network_path = build_fixture_network();
    sirio::ExperimentalSirioNNUE2Runtime runtime{};

    const auto init =
        sirio::initialize_sirio_nnue2_shadow_runtime_for_tests(network_path.string(), runtime);
    assert(init.load_requested);
    assert(init.load_attempted);
    assert(init.load_succeeded);
    assert(init.runtime_status == sirio::ExperimentalSirioNNUE2RuntimeStatus::Loaded);

    sirio::InternalEvalBackendSelection selection{};
    selection.backend = sirio::InternalEvalBackend::ExperimentalSirioNNUE2;

    const std::string fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "8/8/8/8/8/8/6k1/6K1 w - - 0 1",
        "4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1",
    };

    for (const auto &fen : fens) {
        sirio::Board board{fen};
        const auto before = board.to_fen();
        const auto first =
            sirio::evaluate_with_experimental_selector_shadow_for_tests(board, selection, runtime);
        const auto second =
            sirio::evaluate_with_experimental_selector_shadow_for_tests(board, selection, runtime);
        assert(first.actual_backend == sirio::InternalEvalBackend::ExperimentalSirioNNUE2);
        assert(!first.fallback_occurred);
        assert(first.score == second.score);
        assert(board.to_fen() == before);
    }
}

void test_missing_and_malformed_path_report_fallback_metadata() {
    sirio::ExperimentalSirioNNUE2Runtime runtime{};
    const auto missing = sirio::initialize_sirio_nnue2_shadow_runtime_for_tests(
        "/tmp/does_not_exist_sirio_nnue2.bin", runtime);
    assert(missing.load_requested);
    assert(missing.load_attempted);
    assert(!missing.load_succeeded);
    assert(missing.runtime_status == sirio::ExperimentalSirioNNUE2RuntimeStatus::LoadRejected);
    assert(!missing.fallback_reason.empty());
    assert(!runtime.is_loaded());

    const auto malformed_path = repo_root() / "build" / "test_eval_initialization_shadow_v2_malformed.bin";
    {
        std::ofstream malformed{malformed_path};
        malformed << "not_a_sirio_nnue2_minimal_v1_network";
    }

    const auto malformed = sirio::initialize_sirio_nnue2_shadow_runtime_for_tests(
        malformed_path.string(), runtime);
    assert(malformed.load_requested);
    assert(malformed.load_attempted);
    assert(!malformed.load_succeeded);
    assert(malformed.runtime_status == sirio::ExperimentalSirioNNUE2RuntimeStatus::LoadRejected);
    assert(!malformed.fallback_reason.empty());
    assert(!runtime.is_loaded());
}

void test_normal_evaluate_path_and_initialize_remain_stable() {
    const std::string fen = "4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1";
    sirio::Board board{fen};
    sirio::initialize_evaluation(board);
    const auto first = sirio::evaluate(board);
    sirio::initialize_evaluation(board);
    const auto second = sirio::evaluate(board);
    assert(first == second);
}

}  // namespace

void run_evaluation_initialization_shadow_v2_tests() {
    test_default_route_config_does_not_load_runtime();
    test_explicit_network_path_load_and_shadow_wrapper_evaluation();
    test_missing_and_malformed_path_report_fallback_metadata();
    test_normal_evaluate_path_and_initialize_remain_stable();
}
