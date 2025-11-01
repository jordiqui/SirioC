#include <array>
#include <cassert>
#include <filesystem>
#include <numeric>
#include <string>
#include <vector>

#include "sirio/board.hpp"
#include "sirio/nnue/backend.hpp"

namespace {

std::filesystem::path fixture_network_path() {
    std::filesystem::path source_dir = std::filesystem::path(__FILE__).parent_path();
    return source_dir / "data" / "minimal.nnue";
}

void test_evaluate_batch_matches_scalar() {
    sirio::nnue::SingleNetworkBackend backend;
    std::string error;
    std::filesystem::path path = fixture_network_path();
    assert(backend.load(path.string(), &error));

    std::vector<std::string> fens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 2 3",
        "r3k2r/ppp2ppp/8/3P4/8/8/PPP2PPP/R3K2R b KQkq - 1 10",
        "8/8/8/8/8/8/6k1/6K1 w - - 0 1",
    };

    std::vector<sirio::nnue::FeatureState> states;
    std::vector<int> expected;
    states.reserve(fens.size());
    expected.reserve(fens.size());

    for (const auto &fen : fens) {
        sirio::Board board{fen};
        sirio::nnue::FeatureState features = backend.extract_features(board);
        states.push_back(features);
        expected.push_back(backend.evaluate_state(features));
    }

    std::vector<int> batch(states.size());
    backend.evaluate_batch(states, batch);
    for (std::size_t i = 0; i < batch.size(); ++i) {
        assert(batch[i] == expected[i]);
    }
}

void test_evaluate_batch_zero_when_unloaded() {
    std::array<sirio::nnue::FeatureState, 3> states{};
    for (std::size_t i = 0; i < states.size(); ++i) {
        for (std::size_t j = 0; j < states[i].piece_counts.size(); ++j) {
            states[i].piece_counts[j] = static_cast<int>(i * j);
        }
    }

    std::array<int, 3> outputs{};
    sirio::nnue::SingleNetworkBackend backend;
    backend.evaluate_batch(states, outputs);
    for (int value : outputs) {
        assert(value == 0);
    }
}

}  // namespace

void run_nnue_backend_tests() {
    test_evaluate_batch_matches_scalar();
    test_evaluate_batch_zero_when_unloaded();
}

