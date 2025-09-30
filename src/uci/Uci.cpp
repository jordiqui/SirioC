#include "Uci.h"

#include "Options.h"
#include "../nn/nnue.h"

#include "files/fen.h"
#include "pyrrhic/board.h"
#include "pyrrhic/types.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {
constexpr const char* kStartPositionFen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

sirio::pyrrhic::PieceType promotion_type_from_char(char symbol) {
  switch (std::tolower(static_cast<unsigned char>(symbol))) {
    case 'q':
      return sirio::pyrrhic::PieceType::Queen;
    case 'r':
      return sirio::pyrrhic::PieceType::Rook;
    case 'b':
      return sirio::pyrrhic::PieceType::Bishop;
    case 'n':
      return sirio::pyrrhic::PieceType::Knight;
    default:
      throw std::invalid_argument("Invalid promotion piece");
  }
}

std::optional<int> parse_square(const std::string& move, std::size_t offset) {
  if (offset + 1 >= move.size()) return std::nullopt;
  const char file = move[offset];
  const char rank = move[offset + 1];
  if (file < 'a' || file > 'h' || rank < '1' || rank > '8') return std::nullopt;
  const int file_index = file - 'a';
  const int rank_index = rank - '1';
  return sirio::pyrrhic::make_square(file_index, rank_index);
}

std::string square_to_uci(int square) {
  const char file = static_cast<char>('a' + sirio::pyrrhic::file_of(square));
  const char rank = static_cast<char>('1' + sirio::pyrrhic::rank_of(square));
  return std::string{file, rank};
}

void remove_castling_right(std::string& rights, char symbol) {
  rights.erase(std::remove(rights.begin(), rights.end(), symbol), rights.end());
  if (rights.empty()) rights = "-";
}

}  // namespace

namespace core {

class Position {
 public:
  Position() { set_startpos(); }

  bool set_fen(const std::string& fen) {
    try {
      board_ = sirio::files::parse_fen(fen);
      return true;
    } catch (const std::exception&) {
      return false;
    }
  }

  void set_startpos() { board_ = sirio::files::parse_fen(kStartPositionFen); }

  bool do_move(const std::string& uciMove) {
    if (uciMove.size() < 4) return false;
    const auto from_square = parse_square(uciMove, 0);
    const auto to_square = parse_square(uciMove, 2);
    if (!from_square.has_value() || !to_square.has_value()) return false;

    auto moving_piece = board_.piece_at(*from_square);
    if (!moving_piece.has_value()) return false;

    sirio::pyrrhic::Piece piece = *moving_piece;
    const auto moving_color = board_.side_to_move();
    auto rights = board_.castling_rights();
    const auto current_ep = board_.en_passant_square();
    bool is_capture = board_.piece_at(*to_square).has_value();

    // Handle en passant capture
    if (!is_capture && piece.type == sirio::pyrrhic::PieceType::Pawn && current_ep &&
        *current_ep == *to_square) {
      const int capture_square = piece.color == sirio::pyrrhic::Color::White ? *to_square - 8
                                                                             : *to_square + 8;
      board_.set_piece(capture_square, std::nullopt);
      is_capture = true;
    }

    // Handle castling rook moves
    const int from_file = sirio::pyrrhic::file_of(*from_square);
    const int to_file = sirio::pyrrhic::file_of(*to_square);
    const int from_rank = sirio::pyrrhic::rank_of(*from_square);
    const int to_rank = sirio::pyrrhic::rank_of(*to_square);
    const bool is_castling = piece.type == sirio::pyrrhic::PieceType::King &&
                             std::abs(to_file - from_file) == 2 && from_rank == to_rank;

    if (is_castling) {
      if (piece.color == sirio::pyrrhic::Color::White) {
        if (to_file > from_file) {
          // O-O
          const int rook_from = sirio::pyrrhic::make_square(7, 0);
          const int rook_to = sirio::pyrrhic::make_square(5, 0);
          auto rook = board_.piece_at(rook_from);
          if (rook.has_value()) {
            board_.set_piece(rook_to, rook);
            board_.set_piece(rook_from, std::nullopt);
          }
        } else {
          // O-O-O
          const int rook_from = sirio::pyrrhic::make_square(0, 0);
          const int rook_to = sirio::pyrrhic::make_square(3, 0);
          auto rook = board_.piece_at(rook_from);
          if (rook.has_value()) {
            board_.set_piece(rook_to, rook);
            board_.set_piece(rook_from, std::nullopt);
          }
        }
        remove_castling_right(rights, 'K');
        remove_castling_right(rights, 'Q');
      } else {
        if (to_file > from_file) {
          const int rook_from = sirio::pyrrhic::make_square(7, 7);
          const int rook_to = sirio::pyrrhic::make_square(5, 7);
          auto rook = board_.piece_at(rook_from);
          if (rook.has_value()) {
            board_.set_piece(rook_to, rook);
            board_.set_piece(rook_from, std::nullopt);
          }
        } else {
          const int rook_from = sirio::pyrrhic::make_square(0, 7);
          const int rook_to = sirio::pyrrhic::make_square(3, 7);
          auto rook = board_.piece_at(rook_from);
          if (rook.has_value()) {
            board_.set_piece(rook_to, rook);
            board_.set_piece(rook_from, std::nullopt);
          }
        }
        remove_castling_right(rights, 'k');
        remove_castling_right(rights, 'q');
      }
    }

    // Update castling rights for king/rook moves or captures
    if (piece.type == sirio::pyrrhic::PieceType::King) {
      if (piece.color == sirio::pyrrhic::Color::White) {
        remove_castling_right(rights, 'K');
        remove_castling_right(rights, 'Q');
      } else {
        remove_castling_right(rights, 'k');
        remove_castling_right(rights, 'q');
      }
    }

    auto update_rights_for_rook = [&](int square, char symbol) {
      if (*from_square == square || (*to_square == square && is_capture)) {
        remove_castling_right(rights, symbol);
      }
    };

    update_rights_for_rook(sirio::pyrrhic::make_square(0, 0), 'Q');
    update_rights_for_rook(sirio::pyrrhic::make_square(7, 0), 'K');
    update_rights_for_rook(sirio::pyrrhic::make_square(0, 7), 'q');
    update_rights_for_rook(sirio::pyrrhic::make_square(7, 7), 'k');

    // Handle promotion
    sirio::pyrrhic::Piece moved_piece = piece;
    if (uciMove.size() >= 5) {
      try {
        moved_piece.type = promotion_type_from_char(uciMove[4]);
      } catch (const std::invalid_argument&) {
        return false;
      }
    }

    board_.set_piece(*from_square, std::nullopt);
    board_.set_piece(*to_square, moved_piece);

    board_.set_castling_rights(rights);

    board_.set_en_passant_square(std::nullopt);
    if (piece.type == sirio::pyrrhic::PieceType::Pawn && std::abs(to_rank - from_rank) == 2) {
      const int ep_square = piece.color == sirio::pyrrhic::Color::White ? *from_square + 8
                                                                        : *from_square - 8;
      board_.set_en_passant_square(ep_square);
    }

    if (piece.type == sirio::pyrrhic::PieceType::Pawn || is_capture) {
      board_.set_halfmove_clock(0);
    } else {
      board_.set_halfmove_clock(board_.halfmove_clock() + 1);
    }

    if (moving_color == sirio::pyrrhic::Color::Black) {
      board_.set_fullmove_number(board_.fullmove_number() + 1);
    }

    board_.set_side_to_move(sirio::pyrrhic::opposite(moving_color));
    return true;
  }

  std::string best_legal_or_random() const {
    auto moves = board_.generate_basic_moves();
    if (moves.empty()) return {};
    const auto& move = moves.front();
    std::string result = square_to_uci(move.from) + square_to_uci(move.to);
    return result;
  }

  const sirio::pyrrhic::Board& board() const { return board_; }

 private:
  sirio::pyrrhic::Board board_;
};

std::vector<std::string> generate_legal(const Position& pos) {
  std::vector<std::string> moves;
  for (const auto& move : pos.board().generate_basic_moves()) {
    moves.push_back(square_to_uci(move.from) + square_to_uci(move.to));
  }
  return moves;
}

std::string search_bestmove(Position& pos, int /*depth*/) {
  return pos.best_legal_or_random();
}

}  // namespace core

static core::Position g_pos;

void init_options() {
  OptionsMap.clear();
  OptionsMap["Hash"] = Option{Option::SPIN, 1, 4096, 64, false, {}, {}};

  unsigned int detected_threads = std::thread::hardware_concurrency();
  if (detected_threads == 0) detected_threads = 1;
  int threads_default = static_cast<int>(std::min<unsigned int>(detected_threads, 256));
  OptionsMap["Threads"] = Option{Option::SPIN, 1, 256, threads_default, false, {}, {}};

  OptionsMap["Ponder"] = Option{Option::CHECK, 0, 0, 0, false, {}, {}};
  OptionsMap["MultiPV"] = Option{Option::SPIN, 1, 256, 1, false, {}, {}};
  OptionsMap["UseNNUE"] = Option{Option::CHECK, 0, 0, 0, true, {}, [] {
                               // Actual loading handled elsewhere
                             }};
  OptionsMap["EvalFile"] = Option{Option::STRING, 0, 0, 0, false, "", [] {
                               // Actual loading handled elsewhere
                             }};
}

static void load_nn_if_needed() {
  auto it_use = OptionsMap.find("UseNNUE");
  if (it_use == OptionsMap.end()) return;
  if (!it_use->second.b) {
    nnue::state.loaded = false;
    return;
  }

  if (!nnue::state.loaded) {
    const auto it_file = OptionsMap.find("EvalFile");
    const std::string path = it_file != OptionsMap.end() ? it_file->second.s : std::string{};
    if (nnue::load(path)) {
      std::cout << "info string NNUE evaluation using " << nnue::state.path << ' '
                << nnue::state.dims << "\n";
    } else if (!path.empty()) {
      std::cout << "info string NNUE load failed: " << path << "\n";
    }
  }
}

static void cmd_position(std::istringstream& is) {
  std::string token;
  is >> token;
  if (token == "startpos") {
    g_pos.set_startpos();
    if (is >> token && token == "moves") {
      while (is >> token) g_pos.do_move(token);
    }
  } else if (token == "fen") {
    std::string fen, word;
    std::vector<std::string> parts;
    while (is >> word && word != "moves") parts.push_back(word);
    for (std::size_t i = 0; i < parts.size(); ++i) {
      fen += parts[i];
      if (i + 1 < parts.size()) fen += ' ';
    }
    if (!fen.empty()) g_pos.set_fen(fen);
    if (word == "moves") {
      while (is >> token) g_pos.do_move(token);
    }
  }
}

static void cmd_go(std::istringstream& is) {
  std::string token;
  int depth = 1;
  while (is >> token) {
    if (token == "depth") {
      is >> depth;
    }
  }
  std::string bm = core::search_bestmove(g_pos, depth);
  if (bm.empty()) bm = "0000";
  std::cout << "bestmove " << bm << "\n";
}

void uci::loop() {
  init_options();
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);

  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream is(line);
    std::string token;
    if (!(is >> token)) continue;

    if (token == "uci") {
      std::cout << "id name SirioC-0.1.0\n";
      std::cout << "id author SirioC Team\n";
      OptionsMap.printUci();
      std::cout << "uciok\n";
    } else if (token == "isready") {
      load_nn_if_needed();
      std::cout << "readyok\n";
    } else if (token == "setoption") {
      std::string word;
      std::string name;
      std::string value;
      bool reading_name = false;
      bool reading_value = false;
      while (is >> word) {
        if (word == "name") {
          reading_name = true;
          reading_value = false;
          continue;
        }
        if (word == "value") {
          reading_value = true;
          reading_name = false;
          continue;
        }
        if (reading_value) {
          if (!value.empty()) value += ' ';
          value += word;
        } else if (reading_name) {
          if (!name.empty()) name += ' ';
          name += word;
        }
      }
      if (!name.empty()) OptionsMap.set(name, value);
    } else if (token == "ucinewgame") {
      nnue::state.loaded = false;
    } else if (token == "position") {
      cmd_position(is);
    } else if (token == "go") {
      cmd_go(is);
    } else if (token == "stop") {
      // TODO: signal search stop
    } else if (token == "quit") {
      break;
    }
  }
}

