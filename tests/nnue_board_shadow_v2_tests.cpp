#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

#include "sirio/board.hpp"
#include "sirio/move.hpp"
#include "sirio/movegen.hpp"
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

sirio::Move find_legal_move_by_uci(const sirio::Board &board, const std::string &uci) {
    const auto legal_moves = sirio::generate_legal_moves(board);
    for (const auto &move : legal_moves) {
        if (sirio::move_to_uci(move) == uci) {
            return move;
        }
    }
    assert(false && "Required legal move is not available in this board position");
    return sirio::Move{};
}

void assert_shadow_make_unmake_matches_refresh(const sirio::nnue::Nnue2NetworkParameters &net,
                                               const std::string &fen,
                                               const std::string &uci_move,
                                               int cycles = 1) {
    std::string error;
    sirio::Board board{fen};
    const sirio::Move move = find_legal_move_by_uci(board, uci_move);

    sirio::nnue::SirioNNUE2MinimalAccumulator acc{}, rf_before{}, rf_after{};
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(board, net, acc, error));
    assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(board, net, rf_before, error));

    for (int i = 0; i < cycles; ++i) {
        const sirio::Board before = board;
        sirio::Board::UndoState undo{};
        board.make_move(move, undo);

        sirio::nnue::SirioHalfKAv1FeatureDiff diff;
        assert(sirio::nnue::compute_sirio_halfka_v1_feature_diff(before, board, diff));
        assert(!diff.full_refresh_required);

        sirio::nnue::SirioNNUE2MinimalAccumulatorTransition transition;
        assert(sirio::nnue::make_sirio_nnue2_minimal_accumulator_transition(diff, transition));
        assert(transition.valid);
        assert(transition.status == sirio::nnue::SirioNNUE2MinimalAccumulatorTransitionStatus::Valid);

        assert(sirio::nnue::apply_sirio_nnue2_minimal_accumulator_transition(net, transition, acc, error));
        assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(board, net, rf_after, error));
        assert(acc.hidden_pre_activation == rf_after.hidden_pre_activation);

        board.undo_move(move, undo);
        assert(sirio::nnue::undo_sirio_nnue2_minimal_accumulator_transition(net, transition, acc, error));
        assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(board, net, rf_before, error));
        assert(acc.hidden_pre_activation == rf_before.hidden_pre_activation);
        assert(board.to_fen() == fen);
    }
}

void test_board_shadow_quiet_capture_promotion_ep() {
    const auto net = load_fixture_network();
    assert_shadow_make_unmake_matches_refresh(net, "4k3/8/8/8/8/8/4P3/4K3 w - - 0 1", "e2e4", 8);
    assert_shadow_make_unmake_matches_refresh(net, "4k3/8/3p4/4P3/8/8/8/4K3 w - - 0 1", "e5d6", 6);
    assert_shadow_make_unmake_matches_refresh(net, "4k3/6P1/8/8/8/8/8/4K3 w - - 0 1", "g7g8q", 5);
    assert_shadow_make_unmake_matches_refresh(net, "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1", "e5d6", 5);
}

void test_board_shadow_king_move_and_castling_rejection_no_mutation() {
    const auto net = load_fixture_network();
    std::string error;

    for (const auto &scenario : std::vector<std::pair<std::string, std::string>>{
             {"4k3/8/8/8/8/8/8/4K3 w - - 0 1", "e1e2"},
             {"4k2r/8/8/8/8/8/8/4K2R w Kk - 0 1", "e1g1"},
         }) {
        sirio::Board board{scenario.first};
        const auto move = find_legal_move_by_uci(board, scenario.second);
        const sirio::Board before = board;
        sirio::Board::UndoState undo{};
        board.make_move(move, undo);

        sirio::nnue::SirioHalfKAv1FeatureDiff diff;
        assert(sirio::nnue::compute_sirio_halfka_v1_feature_diff(before, board, diff));
        assert(diff.full_refresh_required);

        sirio::nnue::SirioNNUE2MinimalAccumulatorTransition transition{};
        assert(!sirio::nnue::make_sirio_nnue2_minimal_accumulator_transition(diff, transition));
        assert(!transition.valid);
        assert(transition.status == sirio::nnue::SirioNNUE2MinimalAccumulatorTransitionStatus::FullRefreshRequired);

        sirio::nnue::SirioNNUE2MinimalAccumulator acc{}, rf{};
        assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(before, net, acc, error));
        const auto prior = acc.hidden_pre_activation;

        assert(!sirio::nnue::apply_sirio_nnue2_minimal_accumulator_transition(net, transition, acc, error));
        assert(acc.hidden_pre_activation == prior);

        board.undo_move(move, undo);
        assert(board.to_fen() == scenario.first);
        assert(sirio::nnue::refresh_sirio_nnue2_minimal_accumulator(board, net, rf, error));
        assert(acc.hidden_pre_activation == rf.hidden_pre_activation);
    }
}

}  // namespace

void run_nnue_board_shadow_v2_tests() {
    test_board_shadow_quiet_capture_promotion_ep();
    test_board_shadow_king_move_and_castling_rejection_no_mutation();
}
