#include "engine/eval/nnue/evaluator.hpp"

#include "engine/core/board.hpp"
#include "engine/eval/nnue/accumulator.hpp"

#include "nnue.h"
#include "util.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <system_error>
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
        architecture_.clear();
        network_bytes_ = 0;
        return false;
    }

    // Extract header metadata (architecture string) before delegating to the loader.
    file.clear();
    file.seekg(0, std::ios::beg);
    if (!file) {
        network_.reset();
        loaded_path_.clear();
        architecture_.clear();
        network_bytes_ = 0;
        return false;
    }

    std::string architecture;
    std::uint32_t version = 0;
    std::uint32_t checksum = 0;
    std::uint32_t arch_len = 0;

    version = ::nnue::read_little_endian<std::uint32_t>(file);
    checksum = ::nnue::read_little_endian<std::uint32_t>(file);
    arch_len = ::nnue::read_little_endian<std::uint32_t>(file);
    if (!file) {
        network_.reset();
        loaded_path_.clear();
        architecture_.clear();
        network_bytes_ = 0;
        return false;
    }

    (void)version;
    (void)checksum;

    if (arch_len > 0) {
        architecture.resize(static_cast<size_t>(arch_len));
        if (!file.read(architecture.data(), static_cast<std::streamsize>(arch_len))) {
            network_.reset();
            loaded_path_.clear();
            architecture_.clear();
            network_bytes_ = 0;
            return false;
        }
        auto null_pos = architecture.find('\0');
        if (null_pos != std::string::npos) {
            architecture.resize(null_pos);
        }
    }

    file.clear();
    file.seekg(0, std::ios::beg);
    if (!file) {
        network_.reset();
        loaded_path_.clear();
        architecture_.clear();
        network_bytes_ = 0;
        return false;
    }

    auto network = std::make_unique<::nnue::Network>();
    if (!(file >> *network) || !file) {
        network_.reset();
        loaded_path_.clear();
        architecture_.clear();
        network_bytes_ = 0;
        return false;
    }

    network_ = std::move(network);
    loaded_path_ = path;
    architecture_ = std::move(architecture);
    std::error_code ec;
    network_bytes_ = std::filesystem::file_size(path, ec);
    if (ec) {
        network_bytes_ = 0;
    }
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
