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
    const auto out = root / "build" / "test_eval_shadow_integration_minimal_v1.bin";
    const auto script = root / "training" / "nnue" / "scripts" / "export_to_engine_v2.py";
    const std::string cmd = "python " + script.string() + " --output " + out.string();
    assert(std::system(cmd.c_str()) == 0);
    return out;
}

void test_normal_evaluate_output_is_unchanged() {
    const std::string fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "8/8/8/8/8/8/6k1/6K1 w - - 0 1",
        "4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1",
    };

    sirio::ExperimentalSirioNNUE2Runtime runtime{};
    for (const auto &fen : fens) {
        sirio::Board board{fen};
        sirio::initialize_evaluation(board);
        const auto before = sirio::evaluate(board);
        (void)sirio::evaluate_with_sirio_nnue2_shadow_integration_for_tests(board, before, runtime);
        const auto after = sirio::evaluate(board);
        assert(before == after);
    }
}

void test_inactive_runtime_returns_default() {
    sirio::ExperimentalSirioNNUE2Runtime runtime{};
    sirio::Board board{"8/8/8/8/8/8/6k1/6K1 w - - 0 1"};
    constexpr std::int32_t default_score = -19;
    const auto result =
        sirio::evaluate_with_sirio_nnue2_shadow_integration_for_tests(board, default_score, runtime);
    assert(result.score == default_score);
    assert(result.fell_back_to_default);
    assert(!result.used_experimental_runtime);
}

void test_loaded_runtime_matches_p0_26_shadow_hook_and_is_deterministic() {
    const auto network_path = build_fixture_network();
    sirio::ExperimentalSirioNNUE2Runtime runtime{};
    assert(runtime.load_from_file(network_path.string()));

    const std::string fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "8/8/8/8/8/8/6k1/6K1 w - - 0 1",
        "4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1",
    };

    for (const auto &fen : fens) {
        sirio::Board board{fen};
        const auto fen_before = board.to_fen();
        constexpr std::int32_t default_score = 41;

        const auto p026 = sirio::evaluate_with_sirio_nnue2_runtime_for_tests(board, default_score, runtime);
        const auto integrated =
            sirio::evaluate_with_sirio_nnue2_shadow_integration_for_tests(board, default_score, runtime);
        assert(integrated.score == p026.score);
        assert(integrated.used_experimental_runtime == p026.used_experimental_runtime);
        assert(integrated.fell_back_to_default == p026.fell_back_to_default);

        const auto repeat =
            sirio::evaluate_with_sirio_nnue2_shadow_integration_for_tests(board, default_score, runtime);
        assert(repeat.score == integrated.score);
        assert(board.to_fen() == fen_before);
    }
}

}  // namespace

void run_evaluation_shadow_integration_v2_tests() {
    test_normal_evaluate_output_is_unchanged();
    test_inactive_runtime_returns_default();
    test_loaded_runtime_matches_p0_26_shadow_hook_and_is_deterministic();
}
