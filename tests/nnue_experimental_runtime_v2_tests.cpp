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
    const auto out = root / "build" / "test_nnue2_experimental_runtime_minimal_v1.bin";
    const auto script = root / "training" / "nnue" / "scripts" / "export_to_engine_v2.py";
    const std::string cmd = "python " + script.string() + " --output " + out.string();
    assert(std::system(cmd.c_str()) == 0);
    return out;
}

void test_default_constructed_runtime_falls_back() {
    sirio::ExperimentalSirioNNUE2Runtime runtime{};
    assert(!runtime.is_active());
    assert(!runtime.is_loaded());
    assert(runtime.status() == sirio::ExperimentalSirioNNUE2RuntimeStatus::Inactive);

    sirio::Board board{"8/8/8/8/8/8/6k1/6K1 w - - 0 1"};
    constexpr std::int32_t baseline = 42;
    const auto result = runtime.evaluate_with_fallback(board, baseline);
    assert(result.score == baseline);
    assert(result.fell_back_to_default);
    assert(!result.used_experimental_route);
}

void test_runtime_load_and_match_harness_for_fixed_fens() {
    const auto net_path = build_fixture_network();
    sirio::ExperimentalSirioNNUE2Runtime runtime{};
    assert(runtime.load_from_file(net_path.string()));
    assert(runtime.is_active());
    assert(runtime.is_loaded());
    assert(runtime.status() == sirio::ExperimentalSirioNNUE2RuntimeStatus::Loaded);

    const std::string fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "8/8/8/8/8/8/6k1/6K1 w - - 0 1",
        "4k3/8/8/8/3P4/8/8/4K3 b - - 0 1",
    };

    for (const auto &fen : fens) {
        sirio::Board board{fen};
        sirio::initialize_evaluation(board);
        const std::int32_t baseline = sirio::evaluate(board);

        const auto runtime_result = runtime.evaluate_with_fallback(board, baseline);
        const auto harness_result = sirio::evaluate_with_experimental_backend_file_for_tests(
            board, baseline, sirio::EvaluationRoute::ExperimentalSirioNNUE2, net_path.string());
        assert(runtime_result.score == harness_result.score);
        assert(runtime_result.used_experimental_route);
        assert(!runtime_result.fell_back_to_default);

        const auto repeat = runtime.evaluate_with_fallback(board, baseline);
        assert(repeat.score == runtime_result.score);

        const auto fen_before = board.to_fen();
        (void)runtime.evaluate_with_fallback(board, baseline);
        assert(board.to_fen() == fen_before);
    }
}

void test_runtime_load_failures_and_config_activation() {
    sirio::ExperimentalSirioNNUE2Runtime missing{};
    assert(!missing.load_from_file("missing_sirio_runtime_v2.nnue2"));
    assert(missing.is_active());
    assert(!missing.is_loaded());
    assert(missing.status() == sirio::ExperimentalSirioNNUE2RuntimeStatus::LoadRejected);

    const auto bad_path = std::filesystem::temp_directory_path() / "sirio_runtime_bad_v2.nnue2";
    { std::ofstream out(bad_path, std::ios::binary); out << "bad"; }

    sirio::ExperimentalSirioNNUE2Runtime malformed{};
    assert(!malformed.load_from_file(bad_path.string()));
    assert(!malformed.is_loaded());

    sirio::ExperimentalEvaluationConfig config{};
    config.selected_route = sirio::EvaluationRoute::ExperimentalSirioNNUE2;
    config.network_path = build_fixture_network().string();
    sirio::ExperimentalSirioNNUE2Runtime from_config{config};
    assert(from_config.is_active());
    assert(from_config.is_loaded());
}

}  // namespace

void run_nnue_experimental_runtime_v2_tests() {
    test_default_constructed_runtime_falls_back();
    test_runtime_load_and_match_harness_for_fixed_fens();
    test_runtime_load_failures_and_config_activation();
}
