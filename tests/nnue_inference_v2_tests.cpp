#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include "sirio/nnue/backend.hpp"

namespace {

std::filesystem::path repo_root() {
    return std::filesystem::path(__FILE__).parent_path().parent_path();
}

std::filesystem::path build_fixture_network() {
    const auto root = repo_root();
    const auto out = root / "build" / "test_nnue2_minimal_v1.bin";
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

void test_decoded_layout_contract() {
    const auto net = load_fixture_network();
    sirio::nnue::Nnue2MinimalDecodedLayout layout;
    std::string error;
    assert(sirio::nnue::decode_nnue2_minimal_layout(net, layout, error));
    assert(layout.model_layout_name == "SirioNNUE2-MinimalV1");
    assert(layout.model_layout_version == 1);
    assert(layout.feature_set == "SirioHalfKAv1");
    assert(layout.features_per_perspective == 40960);
    assert(layout.accumulator_size == 256);
    assert(layout.hidden1_size == 256);
    assert(layout.hidden2_size == 0);
    assert(layout.output_size == 1);
    assert(layout.activation == "relu");
}

void test_deterministic_inference_fixed_fens() {
    const auto net = load_fixture_network();
    std::string error;
    for (const std::string fen : {
             std::string{"8/8/8/8/8/8/6k1/6K1 w - - 0 1"},
             std::string{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"},
             std::string{"4k3/8/8/8/8/8/4P3/4K3 w - - 0 1"},
         }) {
        sirio::Board board{fen};
        std::int32_t score1 = 0;
        std::int32_t score2 = 0;
        assert(sirio::nnue::evaluate_loaded_nnue2_minimal_v1(board, net, score1, error));
        assert(sirio::nnue::evaluate_loaded_nnue2_minimal_v1(board, net, score2, error));
        assert(score1 == score2);
    }
}

void test_reject_malformed_section_size() {
    auto net = load_fixture_network();
    net.output_weights.pop_back();
    std::int32_t score = 0;
    std::string error;
    sirio::Board board{"8/8/8/8/8/8/6k1/6K1 w - - 0 1"};
    assert(!sirio::nnue::evaluate_loaded_nnue2_minimal_v1(board, net, score, error));
}

}  // namespace

void run_nnue_inference_v2_tests() {
    test_decoded_layout_contract();
    test_deterministic_inference_fixed_fens();
    test_reject_malformed_section_size();
}
