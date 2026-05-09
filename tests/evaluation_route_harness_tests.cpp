#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "sirio/evaluation.hpp"
#include "sirio/evaluation_route.hpp"
#include "sirio/nnue/backend.hpp"

namespace {

std::filesystem::path repo_root() { return std::filesystem::path(__FILE__).parent_path().parent_path(); }

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

        const auto file_routed = sirio::evaluate_with_experimental_backend_file_for_tests(
            board, baseline, sirio::EvaluationRoute::DefaultExisting, "does_not_matter.bin");
        assert(file_routed.score == baseline);
        assert(!file_routed.attempted_file_load);
        assert(!file_routed.file_load_succeeded);
    }
}

void test_experimental_route_matches_p0_14_router() {
    const auto net = load_fixture_network();
    const auto fixture_path = build_fixture_network();
    sirio::Board board{"4k3/8/8/8/3P4/8/8/4K3 b - - 0 1"};

    sirio::use_classical_evaluation();
    sirio::initialize_evaluation(board);
    const std::int32_t baseline = sirio::evaluate(board);

    std::string harness_diag;
    const auto harness = sirio::evaluate_with_experimental_backend_for_tests(
        board, baseline, sirio::EvaluationRoute::ExperimentalSirioNNUE2, &net, &harness_diag);

    std::string file_diag;
    const auto file_harness = sirio::evaluate_with_experimental_backend_file_for_tests(
        board, baseline, sirio::EvaluationRoute::ExperimentalSirioNNUE2, fixture_path.string(),
        &file_diag);

    assert(harness.score == file_harness.score);
    assert(file_harness.used_experimental_route);
    assert(file_harness.attempted_file_load);
    assert(file_harness.file_load_succeeded);
    assert(!file_harness.fell_back_to_default);
}

void test_file_harness_fallback_cases() {
    sirio::Board board{"8/8/8/8/8/8/6k1/6K1 w - - 0 1"};
    constexpr std::int32_t baseline = 17;

    const auto missing = sirio::evaluate_with_experimental_backend_file_for_tests(
        board, baseline, sirio::EvaluationRoute::ExperimentalSirioNNUE2, "missing.nnue2");
    assert(missing.score == baseline);
    assert(missing.fell_back_to_default);
    assert(missing.attempted_file_load);
    assert(!missing.file_load_succeeded);

    const auto temp = std::filesystem::temp_directory_path() / "sirio_eval_route_bad.nnue2";
    {
        std::ofstream out(temp, std::ios::binary);
        out << "not_a_valid_net";
    }
    const auto malformed = sirio::evaluate_with_experimental_backend_file_for_tests(
        board, baseline, sirio::EvaluationRoute::ExperimentalSirioNNUE2, temp.string());
    assert(malformed.score == baseline);
    assert(malformed.fell_back_to_default);

    const auto fake_sf = std::filesystem::temp_directory_path() / "fake_stockfish.nnue";
    {
        std::ofstream out(fake_sf, std::ios::binary);
        out << "NNUE";
    }
    const auto wrong_format = sirio::evaluate_with_experimental_backend_file_for_tests(
        board, baseline, sirio::EvaluationRoute::ExperimentalSirioNNUE2, fake_sf.string());
    assert(wrong_format.score == baseline);
    assert(wrong_format.fell_back_to_default);

    auto net = load_fixture_network();
    net.header.accumulator_size += 1;
    const auto mismatch_path = std::filesystem::temp_directory_path() / "sirio_header_mismatch.nnue2";
    {
        std::ofstream out(mismatch_path, std::ios::binary);
        out.write(reinterpret_cast<const char *>(&net.header), sizeof(net.header));
        out.write(reinterpret_cast<const char *>(net.input_weights.data()),
                  static_cast<std::streamsize>(net.input_weights.size() * sizeof(std::int16_t)));
        out.write(reinterpret_cast<const char *>(net.hidden_bias.data()),
                  static_cast<std::streamsize>(net.hidden_bias.size() * sizeof(std::int16_t)));
        out.write(reinterpret_cast<const char *>(net.output_weights.data()),
                  static_cast<std::streamsize>(net.output_weights.size() * sizeof(std::int16_t)));
        out.write(reinterpret_cast<const char *>(&net.output_bias), sizeof(net.output_bias));
    }
    const auto mismatch = sirio::evaluate_with_experimental_backend_file_for_tests(
        board, baseline, sirio::EvaluationRoute::ExperimentalSirioNNUE2, mismatch_path.string());
    assert(mismatch.score == baseline);
    assert(mismatch.fell_back_to_default);
}

}  // namespace

void run_evaluation_route_harness_tests() {
    test_default_existing_route_matches_evaluate_for_fixed_fens();
    test_experimental_route_matches_p0_14_router();
    test_file_harness_fallback_cases();
}
