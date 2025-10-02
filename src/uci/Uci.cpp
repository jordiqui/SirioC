#include "Uci.h"

#include "Options.h"
#include "../eval.h"
#include "../nn/nnue.h"

extern "C" {
#include "../nn/nnue_paths.h"
}

#include "files/fen.h"
#include "pyrrhic/bench.hpp"
#include "pyrrhic/board.h"
#include "pyrrhic/path.hpp"
#include "pyrrhic/types.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

std::filesystem::path g_engine_dir;

struct NNUEState {
  bool use = true;
  std::string primary;
  std::string secondary;
  bool loaded = false;
} g_nnue;

namespace {
constexpr const char* kStartPositionFen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr const char* kDefaultEvalFile = "nn-1c0000000000.nnue";
constexpr const char* kDefaultEvalFileSmall = "nn-37f18f62d772.nnue";

std::string trim(std::string value) {
  const auto begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) return {};
  const auto end = value.find_last_not_of(" \t\r\n");
  return value.substr(begin, end - begin + 1);
}

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

class ThreadPool {
 public:
  void resize(int requested) {
    int desired = std::max(1, requested);
    if (desired == size_) return;
    workers_.clear();
    for (int i = 0; i < desired - 1; ++i) {
      workers_.emplace_back([](std::stop_token st) {
        while (!st.stop_requested()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
      });
    }
    size_ = desired;
  }

  int size() const { return size_; }

 private:
  int size_ = 1;
  std::vector<std::jthread> workers_;
};

ThreadPool g_thread_pool;
std::filesystem::path g_loaded_nnue_path;
std::filesystem::path g_loaded_secondary_path;
bool g_secondary_loaded = false;

void configure_threads(int requested) {
  const int clamped = std::clamp(requested, 1, 256);
  g_thread_pool.resize(clamped);
  std::cout << "info string Using " << clamped << " thread(s)\n";
}

std::filesystem::path resolve_nnue(const std::string& userPath) {
  namespace fs = std::filesystem;

  std::string query = trim(userPath);
  if (!query.empty() && query.front() == '<' && query.back() == '>') {
    query = trim(query.substr(1, query.size() - 2));
  }
  if (query == "<default>" || query == "<empty>") {
    query.clear();
  }
  if (query.empty()) {
    query = kDefaultEvalFile;
  }
  const auto exe = pathutil::exe_dir();
  std::error_code ec;
  const auto cwd = fs::current_path(ec);

  std::vector<fs::path> candidates;
  const fs::path user(query);

  if (!user.empty()) {
    candidates.push_back(user);
    if (!cwd.empty()) candidates.push_back(cwd / user);

    if (!g_engine_dir.empty()) {
      fs::path probe = g_engine_dir;
      for (int depth = 0; depth < 5 && !probe.empty(); ++depth) {
        candidates.push_back(probe / user);
        candidates.push_back(probe / "resources" / user);
        probe = probe.parent_path();
      }
    }

    fs::path exe_probe = exe;
    for (int depth = 0; depth < 5 && !exe_probe.empty(); ++depth) {
      candidates.push_back(exe_probe / user);
      candidates.push_back(exe_probe / "resources" / user);
      exe_probe = exe_probe.parent_path();
    }
  }

  if (!g_engine_dir.empty()) {
    fs::path probe = g_engine_dir;
    for (int depth = 0; depth < 5 && !probe.empty(); ++depth) {
      candidates.push_back(probe / user);
      candidates.push_back(probe / "resources" / user);
      probe = probe.parent_path();
    }
  }

  if (!cwd.empty()) {
    candidates.push_back(cwd / fs::path("resources") / user);
  }

  fs::path exe_probe = exe;
  for (int depth = 0; depth < 5 && !exe_probe.empty(); ++depth) {
    candidates.push_back(exe_probe / fs::path("resources") / user);
    exe_probe = exe_probe.parent_path();
  }

  auto resolved = pathutil::first_existing(candidates);
  if (!resolved.empty()) {
    return resolved;
  }

  char buffer[4096];
  if (sirio_nnue_locate(query.c_str(), buffer, sizeof(buffer))) {
    return fs::path(buffer);
  }

  return {};
}

void try_load_secondary_nnue() {
  const std::string requested = g_nnue.secondary.empty() ? kDefaultEvalFileSmall : g_nnue.secondary;

  if (requested.empty()) {
    if (g_secondary_loaded) {
      eval_load_small_network(nullptr);
      g_secondary_loaded = false;
      g_loaded_secondary_path.clear();
    }
    return;
  }

  const auto resolved = resolve_nnue(requested);
  if (resolved.empty()) {
    std::cout << "info string NNUE secondary file not found for EvalFileSmall=" << requested
              << "; using material eval\n";
    eval_load_small_network(nullptr);
    g_secondary_loaded = false;
    g_loaded_secondary_path.clear();
    return;
  }

  if (g_secondary_loaded && resolved == g_loaded_secondary_path) {
    return;
  }

  if (eval_load_small_network(resolved.string().c_str()) != 0) {
    std::cout << "info string NNUE secondary loaded: " << resolved.string() << "\n";
    g_secondary_loaded = true;
    g_loaded_secondary_path = resolved;
  } else {
    std::cout << "info string NNUE secondary failed: " << resolved.string() << "; using material eval\n";
    eval_load_small_network(nullptr);
    g_secondary_loaded = false;
    g_loaded_secondary_path.clear();
  }
}

void try_load_nnue() {
  if (!g_nnue.use) {
    if (g_nnue.loaded || nnue::state.loaded) {
      nnue::reset();
    }
    g_nnue.loaded = false;
    g_loaded_nnue_path.clear();
    std::cout << "info string NNUE disabled; using material eval\n";
    try_load_secondary_nnue();
    return;
  }

  const auto resolved = resolve_nnue(g_nnue.primary);
  if (resolved.empty()) {
    if (nnue::has_embedded_default() && nnue::load_default()) {
      g_nnue.loaded = true;
      g_loaded_nnue_path.clear();
      std::cout << "info string NNUE evaluation using embedded default network\n";
      try_load_secondary_nnue();
      return;
    }

    std::cout << "info string NNUE: file not found for EvalFile="
              << (g_nnue.primary.empty() ? std::string{"<default>"} : g_nnue.primary)
              << "; using material eval\n";
    nnue::reset();
    g_nnue.loaded = false;
    g_loaded_nnue_path.clear();
    try_load_secondary_nnue();
    return;
  }

  const bool loaded = nnue::load(resolved.string());
  if (!loaded) {
    if (nnue::has_embedded_default() && nnue::load_default()) {
      g_nnue.loaded = true;
      g_loaded_nnue_path.clear();
      std::cout << "info string NNUE evaluation using embedded default network\n";
      try_load_secondary_nnue();
      return;
    }

    std::cout << "info string NNUE: failed to load " << resolved.string() << "; using material eval\n";
    nnue::reset();
    g_nnue.loaded = false;
    g_loaded_nnue_path.clear();
    try_load_secondary_nnue();
    return;
  }

  g_nnue.loaded = true;
  g_loaded_nnue_path = resolved;

  std::error_code ec;
  const auto bytes = std::filesystem::file_size(resolved, ec);
  std::uint64_t mib = 0;
  if (!ec && bytes > 0) {
    mib = bytes / (1024ULL * 1024ULL);
    if (mib == 0) mib = 1;
  }

  std::cout << "info string NNUE evaluation using " << resolved.string();
  if (mib > 0) {
    std::cout << " (" << mib << "MiB)";
  }
  std::cout << "\n";

  try_load_secondary_nnue();
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
  g_thread_pool.resize(threads_default);

  OptionsMap["Ponder"] = Option{Option::CHECK, 0, 0, 0, false, {}, {}};
  OptionsMap["MultiPV"] = Option{Option::SPIN, 1, 256, 1, false, {}, {}};
  OptionsMap["UseNNUE"] = Option{Option::CHECK, 0, 0, 0, true, {}, {}};

  Option eval_file{Option::STRING, 0, 0, 0, false, "", {}};
  if (!resolve_nnue(kDefaultEvalFile).empty()) {
    eval_file.s = kDefaultEvalFile;
  }

  Option eval_file_small{Option::STRING, 0, 0, 0, false, "", {}};
  if (!resolve_nnue(kDefaultEvalFileSmall).empty()) {
    eval_file_small.s = kDefaultEvalFileSmall;
  }

  OptionsMap["EvalFile"] = eval_file;
  OptionsMap["EvalFileSmall"] = eval_file_small;

  g_nnue.use = OptionsMap["UseNNUE"].b;
  g_nnue.primary = OptionsMap["EvalFile"].s;
  g_nnue.secondary = OptionsMap["EvalFileSmall"].s;
  g_nnue.loaded = false;
  g_loaded_nnue_path.clear();
  g_secondary_loaded = false;
  g_loaded_secondary_path.clear();
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
  std::cout.flush();
}

void uci::loop() {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);

  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream is(line);
    std::string token;
    if (!(is >> token)) continue;

    if (token == "uci") {
      std::cout << "id name SirioC-0.1.0\n";
      std::cout << "id author Jorge Ruiz credits Codex open IA\n";
      OptionsMap.printUci();
      std::cout << "uciok\n";
      std::cout.flush();
    } else if (token == "isready") {
      try_load_nnue();
      std::cout << "readyok\n";
      std::cout.flush();
    } else if (token == "setoption") {
      std::string word;
      std::string name;
      std::string value;
      while (is >> word) {
        if (word == "name") {
          std::string rest;
          std::getline(is, rest);
          const auto value_pos = rest.find(" value ");
          if (value_pos != std::string::npos) {
            name = trim(rest.substr(0, value_pos));
            value = trim(rest.substr(value_pos + 7));
          } else {
            name = trim(rest);
          }
          break;
        }
      }
      if (!name.empty()) {
        OptionsMap.set(name, value);
        if (name == "UseNNUE") {
          g_nnue.use = OptionsMap.at("UseNNUE").b;
          try_load_nnue();
        } else if (name == "EvalFile") {
          g_nnue.primary = OptionsMap.at("EvalFile").s;
          g_nnue.loaded = false;
          try_load_nnue();
        } else if (name == "EvalFileSmall") {
          g_nnue.secondary = OptionsMap.at("EvalFileSmall").s;
          try_load_secondary_nnue();
        } else if (name == "Threads") {
          configure_threads(OptionsMap.at("Threads").val);
        }
      }
    } else if (token == "ucinewgame") {
      nnue::reset();
      nnue::state.loaded = false;
      nnue::state.meta = nnue::Metadata{};
      g_nnue.loaded = false;
      g_loaded_nnue_path.clear();
      g_secondary_loaded = false;
      g_loaded_secondary_path.clear();
      if (g_nnue.use) {
        try_load_nnue();
      } else {
        try_load_secondary_nnue();
      }
    } else if (token == "position") {
      cmd_position(is);
    } else if (token == "go") {
      cmd_go(is);
    } else if (token == "bench") {
      sirio::pyrrhic::run_bench();
      continue;
    } else if (token == "stop") {
      // TODO: signal search stop
    } else if (token == "quit") {
      break;
    }
  }
}

std::filesystem::path uci::resolve_nnue_path_for_tests(const std::string& value) {
  return resolve_nnue(value);
}

