#pragma once

#include "files/pgn_loader.h"
#include "nn/evaluator.h"
#include "pyrrhic/board.h"

#include <optional>
#include <string>
#include <vector>

extern "C" {
#include "pyrrhic/tbprobe.h"
}

namespace sirio::pyrrhic {

struct TablebaseResult {
    int wdl;
    int dtm;
};

class Engine {
public:
    Engine();
    ~Engine();

    void reset();
    void set_position(const std::string& fen);
    std::string current_fen() const;

    int evaluate() const;
    std::vector<Move> generate_moves() const;
    std::optional<Move> suggest_move() const;
    std::optional<TablebaseResult> probe_tablebase() const;

    void load_game(const sirio::files::PgnGame& game);
    void load_game_from_file(const std::string& path);

    bool load_network(const std::string& path);
    bool configure_tablebase(const std::string& path);
    bool has_tablebase() const { return tablebase_ready_; }

    void run_cli(std::istream& input, std::ostream& output);

    const Board& board() const { return board_; }

private:
    Board board_;
    sirio::nn::Evaluator evaluator_;
    sirio::files::PgnGame game_;
    bool tablebase_ready_;
    std::string tablebase_source_;
};

}  // namespace sirio::pyrrhic
