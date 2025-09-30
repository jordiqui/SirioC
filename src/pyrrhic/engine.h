#pragma once

#include "files/pgn_loader.h"
#include "nn/evaluator.h"
#include "pyrrhic/board.h"

#include <optional>
#include <string>
#include <vector>

namespace sirio::pyrrhic {

class Engine {
public:
    Engine();

    void reset();
    void set_position(const std::string& fen);
    std::string current_fen() const;

    int evaluate() const;
    std::vector<Move> generate_moves() const;
    std::optional<Move> suggest_move() const;

    void load_game(const sirio::files::PgnGame& game);
    void load_game_from_file(const std::string& path);

    void run_cli(std::istream& input, std::ostream& output);

    const Board& board() const { return board_; }

private:
    Board board_;
    sirio::nn::Evaluator evaluator_;
    sirio::files::PgnGame game_;
};

}  // namespace sirio::pyrrhic
