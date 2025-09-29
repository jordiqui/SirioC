#include "engine/eval/nnue/evaluator.hpp"

#include "engine/core/board.hpp"
#include "engine/eval/nnue/accumulator.hpp"

#include "nnue.h"

#include <array>
#include <fstream>
#include <utility>
#include <vector>

namespace engine::nnue {

namespace {

::nnue::Piece to_nnue_piece(char piece) {
    switch (piece) {
    case 'P': return ::nnue::WhitePawn;
    case 'N': return ::nnue::WhiteKnight;
    case 'B': return ::nnue::WhiteBishop;
    case 'R': return ::nnue::WhiteRook;
    case 'Q': return ::nnue::WhiteQueen;
    case 'K': return ::nnue::WhiteKing;
    case 'p': return ::nnue::BlackPawn;
    case 'n': return ::nnue::BlackKnight;
    case 'b': return ::nnue::BlackBishop;
    case 'r': return ::nnue::BlackRook;
    case 'q': return ::nnue::BlackQueen;
    case 'k': return ::nnue::BlackKing;
    default: return ::nnue::EmptyPiece;
    }
}

class SirioNNUEInterface {
public:
    explicit SirioNNUEInterface(const engine::Board& board)
        : side_to_move_(board.white_to_move() ? ::nnue::White : ::nnue::Black) {
        const auto& squares = board.squares();
        for (int sq = 0; sq < 64; ++sq) {
            const char piece = squares[static_cast<size_t>(sq)];
            if (piece == '.') continue;
            ::nnue::Piece mapped = to_nnue_piece(piece);
            if (mapped == ::nnue::EmptyPiece) continue;
            if (mapped == ::nnue::WhiteKing) king_squares_[0] = static_cast<::nnue::Square>(sq);
            if (mapped == ::nnue::BlackKing) king_squares_[1] = static_cast<::nnue::Square>(sq);
            pieces_.emplace_back(static_cast<::nnue::Square>(sq), mapped);
        }
        accumulator_.setEmpty();
    }

    ::nnue::Color sideToMove() const noexcept { return side_to_move_; }
    ::nnue::Square kingSquare(::nnue::Color color) const noexcept {
        return king_squares_[static_cast<size_t>(color)];
    }

    ::nnue::Network::AccumulatorType& getAccumulator() noexcept { return accumulator_; }
    const ::nnue::Network::AccumulatorType& getAccumulator() const noexcept { return accumulator_; }

    auto begin() const noexcept { return pieces_.begin(); }
    auto end() const noexcept { return pieces_.end(); }

    unsigned pieceCount() const noexcept { return static_cast<unsigned>(pieces_.size()); }

    unsigned getDirtyNum() const noexcept { return 0; }

    void getDirtyState(size_t, ::nnue::Square& from, ::nnue::Square& to, ::nnue::Piece& piece) const noexcept {
        from = to = ::nnue::InvalidSquare;
        piece = ::nnue::EmptyPiece;
    }

    bool previous() noexcept { return false; }
    bool hasPrevious() const noexcept { return false; }

private:
    ::nnue::Color side_to_move_;
    std::array<::nnue::Square, 2> king_squares_{::nnue::InvalidSquare, ::nnue::InvalidSquare};
    std::vector<std::pair<::nnue::Square, ::nnue::Piece>> pieces_{};
    mutable ::nnue::Network::AccumulatorType accumulator_{};
};

} // namespace

Evaluator::Evaluator() = default;

Evaluator::~Evaluator() = default;

bool Evaluator::load_network(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        network_.reset();
        loaded_path_.clear();
        return false;
    }

    auto network = std::make_unique<::nnue::Network>();
    if (!(file >> *network) || !file) {
        network_.reset();
        loaded_path_.clear();
        return false;
    }

    network_ = std::move(network);
    loaded_path_ = path;
    return true;
}

int Evaluator::eval_cp(const engine::Board& board) const {
    int classical = board.nnue_accumulator().evaluate(board.white_to_move());
    if (!network_) return classical;

    SirioNNUEInterface adapter(board);
    int nn_score = ::nnue::Evaluator<SirioNNUEInterface>::fullEvaluate(*network_, adapter);
    if (nn_score != 0) {
        return nn_score;
    }
    return classical;
}

} // namespace engine::nnue
