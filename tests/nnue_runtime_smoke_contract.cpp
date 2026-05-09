#include <cassert>
#include <cstdint>
#include <string>

#include "sirio/evaluation.hpp"
#include "sirio/evaluation_route.hpp"

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string network_path = argv[1];

    sirio::ExperimentalSirioNNUE2Runtime runtime{};
    assert(runtime.load_from_file(network_path));
    assert(runtime.is_active());
    assert(runtime.is_loaded());
    assert(runtime.status() == sirio::ExperimentalSirioNNUE2RuntimeStatus::Loaded);

    const std::string fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "8/8/8/8/8/8/6k1/6K1 w - - 0 1",
        "4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1",
    };

    for (const auto& fen : fens) {
        sirio::Board board{fen};
        sirio::initialize_evaluation(board);
        const std::int32_t baseline = sirio::evaluate(board);

        const auto first = runtime.evaluate_with_fallback(board, baseline);
        assert(first.used_experimental_route);
        assert(!first.fell_back_to_default);

        const auto second = runtime.evaluate_with_fallback(board, baseline);
        assert(second.score == first.score);
        assert(second.used_experimental_route);
        assert(!second.fell_back_to_default);

        const auto harness = sirio::evaluate_with_experimental_backend_file_for_tests(
            board, baseline, sirio::EvaluationRoute::ExperimentalSirioNNUE2, network_path);
        assert(harness.used_experimental_route);
        assert(!harness.fell_back_to_default);
        assert(harness.score == first.score);

        const auto fen_before = board.to_fen();
        (void)runtime.evaluate_with_fallback(board, baseline);
        assert(board.to_fen() == fen_before);
    }

    return 0;
}
