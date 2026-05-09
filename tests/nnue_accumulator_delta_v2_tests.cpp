#include <cassert>
#include <filesystem>
#include <string>

#include "sirio/nnue/backend.hpp"
#include "sirio/nnue/features.hpp"

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

void verify_delta_equals_refresh(const std::string &before_fen, const std::string &after_fen) {
    const auto net = load_fixture_network();
    const sirio::Board before{before_fen};
    const sirio::Board after{after_fen};
    std::string error;

    sirio::nnue::SirioHalfKAv1FeatureDiff diff;
    assert(sirio::nnue::compute_sirio_halfka_v1_feature_diff(before, after, diff));
    assert(!diff.full_refresh_required);

    sirio::nnue::SirioNNUE2MinimalAccumulator incremental{};
    sirio::nnue::SirioNNUE2MinimalAccumulator refreshed_after{};
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(before, net, incremental, error));
    const auto original_hidden = incremental.hidden_pre_activation;
    assert(sirio::nnue::apply_sirio_nnue2_minimal_accumulator_delta(net, diff, incremental, error));
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(after, net, refreshed_after, error));
    assert(incremental.hidden_pre_activation == refreshed_after.hidden_pre_activation);

    std::int32_t delta_score = 0;
    std::int32_t refresh_score = 0;
    assert(sirio::nnue::evaluate_sirio_nnue2_minimal_accumulator(incremental, net, delta_score, error));
    assert(sirio::nnue::evaluate_sirio_nnue2_minimal_accumulator(refreshed_after, net, refresh_score, error));
    assert(delta_score == refresh_score);

    sirio::nnue::SirioNNUE2MinimalAccumulator repeat{};
    repeat.hidden_pre_activation = original_hidden;
    repeat.valid = true;
    assert(sirio::nnue::apply_sirio_nnue2_minimal_accumulator_delta(net, diff, repeat, error));
    assert(repeat.hidden_pre_activation == incremental.hidden_pre_activation);
}

void test_quiet_move_delta_equals_refresh() {
    verify_delta_equals_refresh("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                                "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
}

void test_capture_like_delta_equals_refresh() {
    verify_delta_equals_refresh("4k3/8/8/3pP3/8/8/8/4K3 w - - 0 1",
                                "4k3/8/3P4/8/8/8/8/4K3 b - - 0 1");
}

void test_promotion_like_delta_equals_refresh() {
    verify_delta_equals_refresh("4k3/6P1/8/8/8/8/8/4K3 w - - 0 1",
                                "4k1Q1/8/8/8/8/8/8/4K3 b - - 0 1");
}

void test_side_to_move_only_empty_delta_no_change() {
    const auto net = load_fixture_network();
    const sirio::Board white{"4k3/8/8/8/8/8/4P3/4K3 w - - 0 1"};
    const sirio::Board black{"4k3/8/8/8/8/8/4P3/4K3 b - - 0 1"};
    std::string error;

    sirio::nnue::SirioHalfKAv1FeatureDiff diff;
    assert(sirio::nnue::compute_sirio_halfka_v1_feature_diff(white, black, diff));
    assert(diff.white_removed.empty() && diff.white_added.empty());
    assert(diff.black_removed.empty() && diff.black_added.empty());

    sirio::nnue::SirioNNUE2MinimalAccumulator acc{};
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(white, net, acc, error));
    const auto before_hidden = acc.hidden_pre_activation;
    assert(sirio::nnue::apply_sirio_nnue2_minimal_accumulator_delta(net, diff, acc, error));
    assert(acc.hidden_pre_activation == before_hidden);
}

void test_refresh_required_rejected_without_mutation() {
    const auto net = load_fixture_network();
    std::string error;
    const sirio::Board before{"4k3/8/8/8/8/8/8/4K3 w - - 0 1"};
    const sirio::Board after{"4k3/8/8/8/8/8/4K3/8 b - - 0 1"};
    sirio::nnue::SirioHalfKAv1FeatureDiff diff;
    assert(sirio::nnue::compute_sirio_halfka_v1_feature_diff(before, after, diff));
    assert(diff.full_refresh_required);

    sirio::nnue::SirioNNUE2MinimalAccumulator acc{};
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(before, net, acc, error));
    const auto prior = acc.hidden_pre_activation;
    assert(!sirio::nnue::apply_sirio_nnue2_minimal_accumulator_delta(net, diff, acc, error));
    assert(acc.hidden_pre_activation == prior);
}

void test_castling_like_rejected_without_mutation() {
    const auto net = load_fixture_network();
    std::string error;
    const sirio::Board before{"4k3/8/8/8/8/8/8/4K2R w - - 0 1"};
    const sirio::Board after{"4k3/8/8/8/8/8/8/5RK1 b - - 0 1"};
    sirio::nnue::SirioHalfKAv1FeatureDiff diff;
    assert(sirio::nnue::compute_sirio_halfka_v1_feature_diff(before, after, diff));
    assert(diff.full_refresh_required);

    sirio::nnue::SirioNNUE2MinimalAccumulator acc{};
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(before, net, acc, error));
    const auto prior = acc.hidden_pre_activation;
    assert(!sirio::nnue::apply_sirio_nnue2_minimal_accumulator_delta(net, diff, acc, error));
    assert(acc.hidden_pre_activation == prior);
}

void test_invalid_accumulator_rejected() {
    const auto net = load_fixture_network();
    sirio::nnue::SirioHalfKAv1FeatureDiff diff;
    diff.full_refresh_required = false;
    std::string error;
    sirio::nnue::SirioNNUE2MinimalAccumulator acc{};
    assert(!sirio::nnue::apply_sirio_nnue2_minimal_accumulator_delta(net, diff, acc, error));
}

void test_invalid_feature_diff_rejected_without_mutation() {
    const auto net = load_fixture_network();
    const sirio::Board board{"4k3/8/8/8/8/8/4P3/4K3 w - - 0 1"};
    std::string error;
    sirio::nnue::SirioNNUE2MinimalAccumulator acc{};
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(board, net, acc, error));
    const auto prior = acc.hidden_pre_activation;

    sirio::nnue::SirioHalfKAv1FeatureDiff diff;
    diff.white_added.push_back(sirio::nnue::SparseFeature{sirio::nnue::kSirioHalfKAv1FeaturesPerPerspective, 1});
    assert(!sirio::nnue::apply_sirio_nnue2_minimal_accumulator_delta(net, diff, acc, error));
    assert(acc.hidden_pre_activation == prior);
}

}  // namespace

void run_nnue_accumulator_delta_v2_tests() {
    test_quiet_move_delta_equals_refresh();
    test_capture_like_delta_equals_refresh();
    test_promotion_like_delta_equals_refresh();
    test_side_to_move_only_empty_delta_no_change();
    test_refresh_required_rejected_without_mutation();
    test_castling_like_rejected_without_mutation();
    test_invalid_accumulator_rejected();
    test_invalid_feature_diff_rejected_without_mutation();
}
