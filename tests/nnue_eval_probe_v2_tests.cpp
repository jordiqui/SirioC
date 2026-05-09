#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "sirio/evaluation.hpp"
#include "sirio/nnue/backend.hpp"

namespace {

std::filesystem::path repo_root() {
    return std::filesystem::path(__FILE__).parent_path().parent_path();
}

std::filesystem::path build_fixture_network() {
    const auto root = repo_root();
    const auto out = root / "build" / "test_nnue2_probe_minimal_v1.bin";
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

std::int32_t probe_white_pov(const std::string &fen,
                             const sirio::nnue::Nnue2NetworkParameters &network) {
    sirio::Board board{fen};
    std::string error;
    std::int32_t score = 0;
    assert(sirio::nnue::evaluate_loaded_nnue2_minimal_v1_probe_white_pov(board, network, score,
                                                                          error));
    return score;
}

void test_probe_white_pov_startpos_side_to_move_invariant() {
    const auto net = load_fixture_network();
    const auto wtm = probe_white_pov("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", net);
    const auto btm = probe_white_pov("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1", net);
    assert(wtm == btm);
}

void test_probe_white_pov_kings_only_side_to_move_invariant() {
    const auto net = load_fixture_network();
    const auto wtm = probe_white_pov("8/8/8/8/8/8/6k1/6K1 w - - 0 1", net);
    const auto btm = probe_white_pov("8/8/8/8/8/8/6k1/6K1 b - - 0 1", net);
    assert(wtm == btm);
}

void test_probe_deterministic_repeated_output() {
    const auto net = load_fixture_network();
    const std::string fen = "4k3/8/8/8/3P4/8/8/4K3 w - - 0 1";
    const auto score_a = probe_white_pov(fen, net);
    const auto score_b = probe_white_pov(fen, net);
    assert(score_a == score_b);
}

void test_probe_rejects_unvalidated_network() {
    auto net = load_fixture_network();
    net.hidden_bias.clear();

    std::string error;
    std::int32_t score = 0;
    sirio::Board board{"8/8/8/8/8/8/6k1/6K1 w - - 0 1"};
    assert(!sirio::nnue::evaluate_loaded_nnue2_minimal_v1_probe_white_pov(board, net, score, error));
}

void test_normal_evaluate_unchanged_by_probe_call() {
    const auto net = load_fixture_network();
    sirio::use_classical_evaluation();

    const std::string fen = "4k3/8/8/8/3P4/8/8/4K3 w - - 0 1";
    sirio::Board board{fen};
    sirio::initialize_evaluation(board);
    const int before = sirio::evaluate(board);

    std::string error;
    std::int32_t probe_score = 0;
    assert(sirio::nnue::evaluate_loaded_nnue2_minimal_v1_probe_white_pov(board, net, probe_score,
                                                                          error));

    sirio::initialize_evaluation(board);
    const int after = sirio::evaluate(board);
    assert(before == after);
}

}  // namespace

void run_nnue_eval_probe_v2_tests() {
    test_probe_white_pov_startpos_side_to_move_invariant();
    test_probe_white_pov_kings_only_side_to_move_invariant();
    test_probe_deterministic_repeated_output();
    test_probe_rejects_unvalidated_network();
    test_normal_evaluate_unchanged_by_probe_call();
}
