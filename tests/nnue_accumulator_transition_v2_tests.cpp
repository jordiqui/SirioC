#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

#include "sirio/nnue/backend.hpp"
#include "sirio/nnue/features.hpp"

namespace {
std::filesystem::path repo_root() { return std::filesystem::path(__FILE__).parent_path().parent_path(); }
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

sirio::nnue::SirioNNUE2MinimalAccumulatorTransition make_transition(const sirio::Board &before,
                                                                    const sirio::Board &after) {
    sirio::nnue::SirioHalfKAv1FeatureDiff diff;
    assert(sirio::nnue::compute_sirio_halfka_v1_feature_diff(before, after, diff));
    sirio::nnue::SirioNNUE2MinimalAccumulatorTransition transition;
    assert(sirio::nnue::make_sirio_nnue2_minimal_accumulator_transition(diff, transition));
    return transition;
}


void test_en_passant_like_transition_apply_undo_no_drift() {
    auto net = load_fixture_network();
    std::string error;
    const sirio::Board white_before{"4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1"};
    const sirio::Board white_after{"4k3/8/3P4/8/8/8/8/4K3 b - - 0 1"};
    const auto white_t = make_transition(white_before, white_after);

    sirio::nnue::SirioNNUE2MinimalAccumulator acc{}, rf{};
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(white_before, net, acc, error));
    assert(sirio::nnue::apply_sirio_nnue2_minimal_accumulator_transition(net, white_t, acc, error));
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(white_after, net, rf, error));
    assert(acc.hidden_pre_activation == rf.hidden_pre_activation);
    assert(sirio::nnue::undo_sirio_nnue2_minimal_accumulator_transition(net, white_t, acc, error));
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(white_before, net, rf, error));
    assert(acc.hidden_pre_activation == rf.hidden_pre_activation);

    const sirio::Board black_before{"4k3/8/8/8/3Pp3/8/8/4K3 b - d3 0 1"};
    const sirio::Board black_after{"4k3/8/8/8/8/3p4/8/4K3 w - - 0 1"};
    const auto black_t = make_transition(black_before, black_after);
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(black_before, net, acc, error));
    assert(sirio::nnue::apply_sirio_nnue2_minimal_accumulator_transition(net, black_t, acc, error));
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(black_after, net, rf, error));
    assert(acc.hidden_pre_activation == rf.hidden_pre_activation);
    assert(sirio::nnue::undo_sirio_nnue2_minimal_accumulator_transition(net, black_t, acc, error));
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(black_before, net, rf, error));
    assert(acc.hidden_pre_activation == rf.hidden_pre_activation);
}
void test_single_transition_apply_undo_no_drift() {
    auto net = load_fixture_network();
    std::string error;
    sirio::Board a{"4k3/8/8/8/8/8/4P3/4K3 w - - 0 1"};
    sirio::Board b{"4k3/8/8/8/4P3/8/8/4K3 b - - 0 1"};
    auto t = make_transition(a, b);

    sirio::nnue::SirioNNUE2MinimalAccumulator acc{}, rf{};
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(a, net, acc, error));
    assert(sirio::nnue::apply_sirio_nnue2_minimal_accumulator_transition(net, t, acc, error));
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(b, net, rf, error));
    assert(acc.hidden_pre_activation == rf.hidden_pre_activation);
    assert(sirio::nnue::undo_sirio_nnue2_minimal_accumulator_transition(net, t, acc, error));
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(a, net, rf, error));
    assert(acc.hidden_pre_activation == rf.hidden_pre_activation);
}

void test_transition_chain_apply_undo_no_drift() {
    auto net = load_fixture_network();
    std::string error;
    std::vector<sirio::Board> p = {sirio::Board{"4k3/8/8/8/8/8/4P3/4K3 w - - 0 1"},
                                   sirio::Board{"4k3/8/8/8/4P3/8/8/4K3 b - - 0 1"},
                                   sirio::Board{"4k3/8/8/4P3/8/8/8/4K3 w - - 0 1"},
                                   sirio::Board{"4k3/8/4P3/8/8/8/8/4K3 b - - 0 1"}};
    std::vector<sirio::nnue::SirioNNUE2MinimalAccumulatorTransition> ts;
    for (int i = 0; i < 3; ++i) ts.push_back(make_transition(p[i], p[i + 1]));

    sirio::nnue::SirioNNUE2MinimalAccumulator acc{}, rf{};
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(p[0], net, acc, error));
    for (int i = 0; i < 3; ++i) {
        assert(sirio::nnue::apply_sirio_nnue2_minimal_accumulator_transition(net, ts[i], acc, error));
        assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(p[i + 1], net, rf, error));
        assert(acc.hidden_pre_activation == rf.hidden_pre_activation);
    }
    for (int i = 2; i >= 0; --i) {
        assert(sirio::nnue::undo_sirio_nnue2_minimal_accumulator_transition(net, ts[i], acc, error));
        assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(p[i], net, rf, error));
        assert(acc.hidden_pre_activation == rf.hidden_pre_activation);
    }
}

void test_rejection_and_noop_paths() {
    auto net = load_fixture_network();
    std::string error;
    sirio::Board before{"4k3/8/8/8/8/8/4P3/4K3 w - - 0 1"};
    sirio::Board stm_only{"4k3/8/8/8/8/8/4P3/4K3 b - - 0 1"};
    sirio::nnue::SirioHalfKAv1FeatureDiff diff;
    assert(sirio::nnue::compute_sirio_halfka_v1_feature_diff(before, stm_only, diff));
    sirio::nnue::SirioNNUE2MinimalAccumulatorTransition t{};
    assert(sirio::nnue::make_sirio_nnue2_minimal_accumulator_transition(diff, t));
    sirio::nnue::SirioNNUE2MinimalAccumulator acc{}, rf{};
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(before, net, acc, error));
    auto prior = acc.hidden_pre_activation;
    assert(sirio::nnue::apply_sirio_nnue2_minimal_accumulator_transition(net, t, acc, error));
    assert(acc.hidden_pre_activation == prior);
    assert(sirio::nnue::undo_sirio_nnue2_minimal_accumulator_transition(net, t, acc, error));
    assert(acc.hidden_pre_activation == prior);

    sirio::Board km_after{"4k3/8/8/8/8/8/4K3/8 b - - 0 1"};
    assert(sirio::nnue::compute_sirio_halfka_v1_feature_diff(before, km_after, diff));
    assert(!sirio::nnue::make_sirio_nnue2_minimal_accumulator_transition(diff, t));
    assert(!sirio::nnue::apply_sirio_nnue2_minimal_accumulator_transition(net, t, acc, error));
    assert(acc.hidden_pre_activation == prior);

    t.valid = true;
    t.status = sirio::nnue::SirioNNUE2MinimalAccumulatorTransitionStatus::Valid;
    t.white_added.push_back({sirio::nnue::kSirioHalfKAv1FeaturesPerPerspective, 1});
    assert(!sirio::nnue::apply_sirio_nnue2_minimal_accumulator_transition(net, t, acc, error));
    assert(acc.hidden_pre_activation == prior);

    sirio::nnue::SirioNNUE2MinimalAccumulator invalid_acc{};
    assert(!sirio::nnue::apply_sirio_nnue2_minimal_accumulator_transition(net, t, invalid_acc, error));

    sirio::nnue::SirioNNUE2MinimalAccumulatorTransition invalid_ep_like{};
    invalid_ep_like.valid = true;
    invalid_ep_like.status = sirio::nnue::SirioNNUE2MinimalAccumulatorTransitionStatus::Valid;
    invalid_ep_like.white_removed.push_back({0, 1});
    invalid_ep_like.black_removed.push_back({sirio::nnue::kSirioHalfKAv1FeaturesPerPerspective, 1});
    assert(!sirio::nnue::apply_sirio_nnue2_minimal_accumulator_transition(net, invalid_ep_like, acc, error));
    assert(acc.hidden_pre_activation == prior);
}

}  // namespace

void run_nnue_accumulator_transition_v2_tests() {
    test_en_passant_like_transition_apply_undo_no_drift();
    test_single_transition_apply_undo_no_drift();
    test_transition_chain_apply_undo_no_drift();
    test_rejection_and_noop_paths();
}
