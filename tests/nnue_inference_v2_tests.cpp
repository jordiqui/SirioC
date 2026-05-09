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

void test_accumulator_refresh_equality_fixed_fens() {
    const auto net = load_fixture_network();
    std::string error;
    for (const std::string fen : {
             std::string{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"},
             std::string{"8/8/8/8/8/8/6k1/6K1 w - - 0 1"},
             std::string{"4k3/8/8/8/8/8/4P3/4K3 w - - 0 1"},
             std::string{"r3k2r/pp3ppp/2n1bn2/3p4/3P4/2N1PN2/PP3PPP/R3K2R w KQkq - 0 1"},
         }) {
        sirio::Board board{fen};
        std::int32_t direct_score = 0;
        assert(sirio::nnue::evaluate_loaded_nnue2_minimal_v1(board, net, direct_score, error));

        sirio::nnue::SirioNNUE2MinimalAccumulator accumulator{};
        assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(board, net, accumulator, error));
        std::int32_t accum_score = 0;
        assert(sirio::nnue::evaluate_sirio_nnue2_minimal_accumulator(accumulator, net, accum_score,
                                                                      error));
        assert(direct_score == accum_score);
    }
}

void test_accumulator_refresh_deterministic_repeat() {
    const auto net = load_fixture_network();
    const sirio::Board board{"r3k2r/pp3ppp/2n1bn2/3p4/3P4/2N1PN2/PP3PPP/R3K2R w KQkq - 0 1"};
    std::string error;

    sirio::nnue::SirioNNUE2MinimalAccumulator a1{};
    sirio::nnue::SirioNNUE2MinimalAccumulator a2{};
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(board, net, a1, error));
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(board, net, a2, error));
    assert(a1.valid && a2.valid);
    assert(a1.hidden_pre_activation == a2.hidden_pre_activation);
}

void test_accumulator_clear_and_reinitialize() {
    const auto net = load_fixture_network();
    std::string error;
    sirio::nnue::SirioNNUE2MinimalAccumulator accumulator{};
    const sirio::Board board{"4k3/8/8/8/8/8/4P3/4K3 w - - 0 1"};

    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(board, net, accumulator, error));
    assert(accumulator.valid);
    assert(!accumulator.hidden_pre_activation.empty());
    accumulator.clear();
    assert(!accumulator.valid);
    assert(accumulator.hidden_pre_activation.empty());

    std::int32_t score = 0;
    assert(!sirio::nnue::evaluate_sirio_nnue2_minimal_accumulator(accumulator, net, score, error));
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(board, net, accumulator, error));
    assert(accumulator.valid);
}

void test_reject_malformed_section_size() {
    auto net = load_fixture_network();
    net.output_weights.pop_back();
    std::int32_t score = 0;
    std::string error;
    sirio::Board board{"8/8/8/8/8/8/6k1/6K1 w - - 0 1"};
    assert(!sirio::nnue::evaluate_loaded_nnue2_minimal_v1(board, net, score, error));
}

void test_accumulator_reject_unvalidated_network() {
    sirio::nnue::Nnue2NetworkParameters net;
    sirio::Board board{"8/8/8/8/8/8/6k1/6K1 w - - 0 1"};
    sirio::nnue::SirioNNUE2MinimalAccumulator accumulator{};
    std::string error;
    assert(!sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(board, net, accumulator, error));
}

void test_white_pov_accumulator_side_to_move_invariant() {
    const auto net = load_fixture_network();
    std::string error;
    const sirio::Board white{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
    const sirio::Board black{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1"};

    sirio::nnue::SirioNNUE2MinimalAccumulator white_acc{};
    sirio::nnue::SirioNNUE2MinimalAccumulator black_acc{};
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(white, net, white_acc, error));
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(black, net, black_acc, error));

    std::int32_t white_score = 0;
    std::int32_t black_score = 0;
    assert(sirio::nnue::evaluate_sirio_nnue2_minimal_accumulator(white_acc, net, white_score, error));
    assert(sirio::nnue::evaluate_sirio_nnue2_minimal_accumulator(black_acc, net, black_score, error));
    assert(white_score == black_score);
}

}  // namespace

void run_nnue_inference_v2_tests() {
    test_decoded_layout_contract();
    test_deterministic_inference_fixed_fens();
    test_accumulator_refresh_equality_fixed_fens();
    test_accumulator_refresh_deterministic_repeat();
    test_accumulator_clear_and_reinitialize();
    test_reject_malformed_section_size();
    test_accumulator_reject_unvalidated_network();
    test_white_pov_accumulator_side_to_move_invariant();
}
