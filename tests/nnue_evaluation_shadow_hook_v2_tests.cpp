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
    const auto out = root / "build" / "test_nnue2_eval_shadow_hook_minimal_v1.bin";
    const auto script = root / "training" / "nnue" / "scripts" / "export_to_engine_v2.py";
    const std::string cmd = "python " + script.string() + " --output " + out.string();
    assert(std::system(cmd.c_str()) == 0);
    return out;
}

void test_inactive_runtime_falls_back_to_provided_default() {
    sirio::ExperimentalSirioNNUE2Runtime runtime{};
    sirio::Board board{"8/8/8/8/8/8/6k1/6K1 w - - 0 1"};
    constexpr std::int32_t provided_default = -77;
    const auto result = sirio::evaluate_with_sirio_nnue2_runtime_for_tests(board, provided_default, runtime);
    assert(result.score == provided_default);
    assert(result.fell_back_to_default);
    assert(!result.used_experimental_runtime);
    assert(!result.runtime_active);
    assert(!result.runtime_loaded);
}

void test_loaded_runtime_matches_direct_runtime_and_is_deterministic() {
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
        constexpr std::int32_t provided_default = 123;

        const auto direct = runtime.evaluate_with_fallback(board, provided_default);
        const auto hooked =
            sirio::evaluate_with_sirio_nnue2_runtime_for_tests(board, provided_default, runtime);
        assert(hooked.score == direct.score);
        assert(hooked.used_experimental_runtime == direct.used_experimental_route);
        assert(hooked.fell_back_to_default == direct.fell_back_to_default);

        const auto repeat =
            sirio::evaluate_with_sirio_nnue2_runtime_for_tests(board, provided_default, runtime);
        assert(repeat.score == hooked.score);
        assert(board.to_fen() == fen_before);
    }
}

void test_normal_evaluate_remains_unchanged() {
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
        sirio::initialize_evaluation(board);
        const auto eval_before = sirio::evaluate(board);
        (void)sirio::evaluate_with_sirio_nnue2_runtime_for_tests(board, eval_before, runtime);
        const auto eval_after = sirio::evaluate(board);
        assert(eval_after == eval_before);
    }
}

}  // namespace

void run_nnue_evaluation_shadow_hook_v2_tests() {
    test_inactive_runtime_falls_back_to_provided_default();
    test_loaded_runtime_matches_direct_runtime_and_is_deterministic();
    test_normal_evaluate_remains_unchanged();
}
