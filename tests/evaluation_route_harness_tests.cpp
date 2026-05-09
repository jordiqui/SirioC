#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "sirio/evaluation.hpp"
#include "sirio/evaluation_route.hpp"
#include "sirio/nnue/backend.hpp"

namespace {

std::filesystem::path repo_root() {
    return std::filesystem::path(__FILE__).parent_path().parent_path();
}

std::filesystem::path build_fixture_network() {
    const auto root = repo_root();
    const auto out = root / "build" / "test_nnue2_eval_route_minimal_v1.bin";
    const auto script = root / "training" / "nnue" / "scripts" / "export_to_engine_v2.py";
    const std::string cmd = "python " + script.string() + " --output " + out.string();
    assert(std::system(cmd.c_str()) == 0);
    return out;
}

sirio::nnue::Nnue2NetworkParameters load_fixture_network() {
    std::string error;
    sirio::nnue::Nnue2NetworkParameters net;
    assert(sirio::nnue::load_nnue2_network_file(build_fixture_network().string(), net, error));
    return net;
}

void test_default_existing_route_matches_evaluate_for_fixed_fens() {
    sirio::use_classical_evaluation();
    const std::string fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "8/8/8/8/8/8/6k1/6K1 w - - 0 1",
        "4k3/8/8/8/3P4/8/8/4K3 b - - 0 1",
    };

    for (const auto &fen : fens) {
        sirio::Board board{fen};
        sirio::initialize_evaluation(board);
        const auto baseline = static_cast<std::int32_t>(sirio::evaluate(board));

        const auto routed = sirio::evaluate_with_experimental_backend_for_tests(
            board, sirio::EvaluationRoute::DefaultExisting, nullptr);
        assert(routed.score == baseline);
        assert(routed.used_default_route);
        assert(!routed.used_experimental_route);
        assert(!routed.fell_back_to_default);
    }
}

void test_experimental_route_matches_p0_14_router() {
    const auto net = load_fixture_network();
    sirio::Board board{"4k3/8/8/8/3P4/8/8/4K3 b - - 0 1"};

    sirio::use_classical_evaluation();
    sirio::initialize_evaluation(board);
    const std::int32_t baseline = sirio::evaluate(board);

    std::string harness_diag;
    const auto harness = sirio::evaluate_with_experimental_backend_for_tests(
        board, baseline, sirio::EvaluationRoute::ExperimentalSirioNNUE2, &net, &harness_diag);

    std::string gate_diag;
    const auto gate = sirio::nnue::route_experimental_nnue2_evaluation(
        board, sirio::nnue::ExperimentalEvalBackend::ExperimentalSirioNNUE2, baseline, &net,
        &gate_diag);

    assert(harness.score == gate.score);
    assert(harness.used_experimental_route);
    assert(!harness.fell_back_to_default);
}

void test_experimental_route_missing_network_falls_back() {
    sirio::Board board{"8/8/8/8/8/8/6k1/6K1 w - - 0 1"};
    constexpr std::int32_t baseline = 17;

    const auto harness = sirio::evaluate_with_experimental_backend_for_tests(
        board, baseline, sirio::EvaluationRoute::ExperimentalSirioNNUE2, nullptr);
    assert(harness.score == baseline);
    assert(harness.used_default_route);
    assert(!harness.used_experimental_route);
    assert(harness.fell_back_to_default);
}

void test_experimental_route_invalid_network_falls_back() {
    auto net = load_fixture_network();
    net.hidden_bias.clear();

    sirio::Board board{"8/8/8/8/8/8/6k1/6K1 w - - 0 1"};
    constexpr std::int32_t baseline = -41;

    const auto harness = sirio::evaluate_with_experimental_backend_for_tests(
        board, baseline, sirio::EvaluationRoute::ExperimentalSirioNNUE2, &net);
    assert(harness.score == baseline);
    assert(harness.used_default_route);
    assert(!harness.used_experimental_route);
    assert(harness.fell_back_to_default);
}

}  // namespace

void run_evaluation_route_harness_tests() {
    test_default_existing_route_matches_evaluate_for_fixed_fens();
    test_experimental_route_matches_p0_14_router();
    test_experimental_route_missing_network_falls_back();
    test_experimental_route_invalid_network_falls_back();
}
