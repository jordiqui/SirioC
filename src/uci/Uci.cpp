#include "Uci.h"

#include "Options.h"
#include "../eval.h"
#include "../nn/nnue.h"

#include "files/fen.h"
#include "pyrrhic/board.h"
#include "pyrrhic/types.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

std::filesystem::path g_engine_dir;

namespace {
constexpr const char* kStartPositionFen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr const char* kDefaultEvalFile = "sirio_default.nnue";
constexpr const char* kDefaultEvalFileSmall = "sirio_small.nnue";

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

std::filesystem::path resolve_nnue_path(const std::string& value) {
  if (value.empty()) return {};

  std::error_code ec;
  std::filesystem::path candidate(value);
  if (candidate.is_absolute()) {
    if (std::filesystem::exists(candidate, ec)) return candidate;
    return {};
  }

  std::vector<std::filesystem::path> resolved;
  std::unordered_set<std::string> seen;

  auto push_if_exists = [&](const std::filesystem::path& path) {
    if (path.empty()) return;
    std::error_code exists_ec;
    if (!std::filesystem::exists(path, exists_ec)) return;
    const std::string key = path.lexically_normal().generic_string();
    if (seen.insert(key).second) {
      resolved.push_back(path);
    }
  };

  auto consider_base = [&](const std::filesystem::path& base, bool include_resources) {
    if (base.empty()) return;
    push_if_exists(base / candidate);
    if (include_resources) {
      push_if_exists(base / "resources" / candidate);
    }
  };

  push_if_exists(candidate);

  auto cwd = std::filesystem::current_path(ec);
  if (!ec) {
    consider_base(cwd, true);
  }

  const char* env_resource_dir = std::getenv("SIRIOC_RESOURCE_DIR");
  if (env_resource_dir && *env_resource_dir) {
    consider_base(env_resource_dir, true);
  }
  const char* legacy_env_dir = std::getenv("SIRIO_RESOURCE_DIR");
  if (legacy_env_dir && *legacy_env_dir) {
    consider_base(legacy_env_dir, true);
  }

  if (!g_engine_dir.empty()) {
    std::filesystem::path probe = g_engine_dir;
    for (int depth = 0; depth < 5 && !probe.empty(); ++depth) {
      consider_base(probe, true);
      probe = probe.parent_path();
    }
  }

  if (!resolved.empty()) {
    return resolved.front();
  }

  return {};
}

std::string format_source_kind(nnue::Metadata::SourceType type) {
  using Source = nnue::Metadata::SourceType;
  switch (type) {
    case Source::File:
      return "file";
    case Source::Embedded:
      return "<embedded>";
    case Source::Memory:
      return "memory";
    case Source::Unknown:
    default:
      return "unknown";
  }
}

void report_primary_success(const nnue::Metadata& meta,
                            const std::filesystem::path& resolved_path) {
  const std::string display_path = !meta.source.empty()
                                       ? meta.source
                                       : (!resolved_path.empty() ? resolved_path.string()
                                                                 : std::string{"<unknown>"});

  std::cout << "info string NNUE evaluation using " << display_path;
  std::ostringstream details;
  bool has_details = false;
  if (meta.size_bytes > 0) {
    const std::uint64_t mib = meta.size_bytes / (1024ULL * 1024ULL);
    details << (mib > 0 ? mib : 1) << "MiB";
    has_details = true;
  }
  if (!meta.dimensions.empty()) {
    if (has_details) details << ", ";
    details << meta.dimensions;
    has_details = true;
  }
  if (has_details) {
    std::cout << " (" << details.str() << ')';
  }
  std::cout << "\n";
  std::cout << "info string     source: " << format_source_kind(meta.source_type) << "\n";
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

std::filesystem::path locate_bench_file() {
  std::vector<std::filesystem::path> candidates;
  if (!g_engine_dir.empty()) {
    candidates.push_back(g_engine_dir / "resources" / "bench.fens");
    const auto parent = g_engine_dir.parent_path();
    if (!parent.empty()) {
      candidates.push_back(parent / "resources" / "bench.fens");
    }
  }
  candidates.emplace_back("resources/bench.fens");
  candidates.emplace_back("../resources/bench.fens");

  std::error_code ec;
  for (const auto& candidate : candidates) {
    if (std::filesystem::exists(candidate, ec) &&
        std::filesystem::is_regular_file(candidate, ec)) {
      return candidate;
    }
  }
  return {};
}

void print_info_string(const std::string& message) {
  std::cout << "info string " << message << "\n";
  std::cout.flush();
}

struct BenchLimits {
  int depth = 12;
  int movetime_ms = 0;
};

BenchLimits parse_bench_limits(std::istringstream& is) {
  BenchLimits limits;
  std::string token;
  while (is >> token) {
    if (token == "depth") {
      is >> limits.depth;
    } else if (token == "movetime") {
      is >> limits.movetime_ms;
    }
  }
  return limits;
}

void cmd_bench(std::istringstream& is) {
  const BenchLimits limits = parse_bench_limits(is);
  if (limits.movetime_ms > 0) {
    print_info_string("bench movetime option is currently ignored");
  }
  auto bench_path = locate_bench_file();
  if (bench_path.empty()) {
    print_info_string("Failed to open resources/bench.fens");
    return;
  }

  std::ifstream bench_file(bench_path);
  if (!bench_file.is_open()) {
    print_info_string("Failed to open resources/bench.fens");
    return;
  }

  std::uint64_t total_nodes = 0;
  std::uint64_t total_time = 0;
  int positions = 0;
  std::string line;

  while (std::getline(bench_file, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#') continue;

    core::Position pos;
    if (!pos.set_fen(line)) {
      print_info_string("Invalid FEN in bench file: " + line);
      continue;
    }

    const int requested_depth = limits.depth > 0 ? limits.depth : 1;

    const auto start = std::chrono::steady_clock::now();
    std::string best = core::search_bestmove(pos, requested_depth);
    const auto finish = std::chrono::steady_clock::now();
    std::uint64_t elapsed_ms =
        static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count());
    if (elapsed_ms == 0) elapsed_ms = 1;

    const auto legal_moves = core::generate_legal(pos);
    std::uint64_t nodes = static_cast<std::uint64_t>(legal_moves.size());

    ++positions;
    total_nodes += nodes;
    total_time += elapsed_ms;

    if (best.empty()) best = "0000";

    std::cout << "bench position " << positions << " bestmove " << best << " nodes " << nodes
              << " time " << elapsed_ms << "\n";
    std::cout.flush();
  }

  if (positions == 0) {
    std::cout << "bench summary positions 0 time 0 nodes 0 nps 0\n";
    std::cout.flush();
    return;
  }

  if (total_time == 0) total_time = 1;
  const std::uint64_t nps = (total_nodes * 1000ULL) / total_time;

  std::cout << "bench summary positions " << positions << " time " << total_time << " nodes "
            << total_nodes << " nps " << nps << "\n";
  std::cout.flush();

  g_pos.set_startpos();
}

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
  Option eval_file{Option::STRING, 0, 0, 0, false, "", {}};
  if (!resolve_nnue_path(kDefaultEvalFile).empty()) {
    eval_file.s = kDefaultEvalFile;
  }
  Option eval_file_small{Option::STRING, 0, 0, 0, false, "", {}};
  if (!resolve_nnue_path(kDefaultEvalFileSmall).empty()) {
    eval_file_small.s = kDefaultEvalFileSmall;
  }
  OptionsMap["EvalFile"] = std::move(eval_file);
  OptionsMap["EvalFileSmall"] = std::move(eval_file_small);
}

static void try_load_nnue() {
  static std::string last_primary_request;
  static std::string last_small_request;
  static bool last_use_nnue = false;
  static bool reported_primary_failure = false;
  static bool reported_small_failure = false;

  auto it_use = OptionsMap.find("UseNNUE");
  if (it_use == OptionsMap.end()) return;

  const bool use_nnue = it_use->second.b;
  if (!use_nnue) {
    if (last_use_nnue) {
      if (nnue::state.loaded) {
        nnue::reset();
      }
      std::cout << "info string NNUE disabled by option\n";
      reported_primary_failure = false;
      reported_small_failure = false;
    }
    last_use_nnue = false;
    last_primary_request.clear();
    last_small_request.clear();
    return;
  }

  last_use_nnue = true;

  const auto it_file = OptionsMap.find("EvalFile");
  const std::string requested_primary = it_file != OptionsMap.end() ? it_file->second.s : std::string{};
  if (!nnue::state.loaded || requested_primary != last_primary_request) {
    last_primary_request = requested_primary;
    bool loaded = false;
    std::filesystem::path resolved_primary;
    if (!requested_primary.empty()) {
      resolved_primary = resolve_nnue_path(requested_primary);
      if (!resolved_primary.empty()) {
        std::cout << "info string NNUE: trying main " << resolved_primary.string() << "\n";
        loaded = nnue::load(resolved_primary.string());
      } else {
        std::cout << "info string NNUE: file not found " << requested_primary << "\n";
      }
    }
    if (!loaded && requested_primary.empty() && nnue::has_embedded_default()) {
      std::cout << "info string NNUE: trying embedded default\n";
      loaded = nnue::load_default();
    }
    if (loaded) {
      reported_primary_failure = false;
      report_primary_success(nnue::state.meta, resolved_primary);
    } else {
      if (!reported_primary_failure) {
        std::cout << "info string NNUE: no network loaded; using material eval\n";
      }
      reported_primary_failure = true;
      nnue::reset();
    }
  }

  const auto it_small = OptionsMap.find("EvalFileSmall");
  const std::string requested_small = it_small != OptionsMap.end() ? it_small->second.s : std::string{};
  if (requested_small != last_small_request) {
    last_small_request = requested_small;
    if (requested_small.empty()) {
      eval_load_small_network(nullptr);
      reported_small_failure = false;
    } else {
      const auto resolved_small = resolve_nnue_path(requested_small);
      if (!resolved_small.empty()) {
        std::cout << "info string NNUE: trying secondary " << resolved_small.string() << "\n";
        const bool loaded_small = eval_load_small_network(resolved_small.string().c_str()) != 0;
        if (loaded_small) {
          std::cout << "info string NNUE secondary loaded: " << resolved_small.string() << "\n";
          reported_small_failure = false;
        } else {
          std::cout << "info string NNUE secondary failed: " << resolved_small.string() << "\n";
          reported_small_failure = true;
        }
      } else {
        if (!reported_small_failure) {
          std::cout << "info string NNUE secondary failed: " << requested_small << "\n";
        }
        reported_small_failure = true;
      }
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
        if (name == "EvalFile" || name == "EvalFileSmall" || name == "UseNNUE") {
          try_load_nnue();
        }
      }
    } else if (token == "ucinewgame") {
      nnue::state.loaded = false;
      nnue::state.meta = nnue::Metadata{};
    } else if (token == "position") {
      cmd_position(is);
    } else if (token == "go") {
      cmd_go(is);
    } else if (token == "bench") {
      cmd_bench(is);
    } else if (token == "stop") {
      // TODO: signal search stop
    } else if (token == "quit") {
      break;
    }
  }
}

std::filesystem::path uci::resolve_nnue_path_for_tests(const std::string& value) {
  return resolve_nnue_path(value);
}

