#include "bench.hpp"

#include "files/fen.h"
#include "pyrrhic/board.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

namespace {

constexpr const char* kStartPositionFen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

sirio::pyrrhic::Board load_board(const std::string& fen) {
  if (fen == "startpos") {
    return sirio::files::parse_fen(kStartPositionFen);
  }
  return sirio::files::parse_fen(fen);
}

struct SearchLimits {
  int depth = 10;
};

struct SearchResult {
  std::uint64_t nodes = 0;
};

SearchResult search_position(const sirio::pyrrhic::Board& board, const SearchLimits& limits) {
  SearchResult result{};
  const auto moves = board.generate_basic_moves();
  result.nodes = static_cast<std::uint64_t>(moves.size()) * static_cast<std::uint64_t>(limits.depth);
  return result;
}

}  // namespace

namespace sirio::pyrrhic {

void run_bench() {
  static const std::array<const char*, 16> kBenchFens = {
      "startpos",
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "r3k2r/pppq1ppp/2n1pn2/8/2BP4/2N2N2/PPP2PPP/R2Q1RK1 w kq - 0 1",
      "2r2rk1/1p1bbppp/p1n1pn2/q3N3/3P4/2N1P2P/PPQ2PP1/2KR1B1R w - - 0 1",
      "r4rk1/1pp1qppp/p1npbn2/4p3/2P1P3/1P1P1N1P/PB1N1PP1/R2QR1K1 w - - 0 1",
      "4rrk1/pp1q1ppp/2n1p3/2bpP3/3N4/2N1Q3/PP3PPP/R3R1K1 w - - 0 1",
      "r1bq1rk1/pp1nbppp/2p1pn2/3p4/3P1B2/2N1PN2/PPQ2PPP/2KR1B1R w - - 0 1",
      "2kr3r/ppqb1pp1/2n1pn1p/2pp4/3P4/2P1PN2/PP1N1PPP/R1BQR1K1 w - - 0 1",
      "r1bq1rk1/pp3pp1/2p1pn1p/2bp4/3P4/2P1PN2/PP1N1PPP/R1BQR1K1 w - - 0 1",
      "4rrk1/pp1q1ppp/2n1p3/2bpP3/3N1Q2/2N5/PP3PPP/R3R1K1 w - - 0 1",
      "rnb1k2r/pppp1ppp/4pn2/q7/2BP4/2N2N2/PPP2PPP/R1BQK2R w KQkq - 0 1",
      "r1b1k2r/pppp1ppp/2n2n2/q7/2BP4/2N2N2/PPP2PPP/R1BQ1RK1 w kq - 0 1",
      "rnbq1rk1/ppp1bppp/3ppn2/8/2BP4/2N2N2/PPP2PPP/R1BQ1RK1 w - - 0 1",
      "r1bq1rk1/ppp1bppp/2nppn2/8/2BP4/2N1PN2/PPPQ1PPP/R1B2RK1 w - - 0 1",
      "2r2rk1/1p1bbppp/p1n1pn2/q3N3/3P4/2N1P1PP/PPQ2PB1/2KR3R w - - 0 1",
      "r2q1rk1/1pp2pp1/p1npbn1p/3Np3/2P1P3/2N1B1PP/PPQ2PB1/2KR3R w - - 0 1",
  };

  std::uint64_t total_nodes = 0;
  auto start = std::chrono::steady_clock::now();

  for (const char* fen : kBenchFens) {
    sirio::pyrrhic::Board board = load_board(fen);
    SearchLimits limits{};
    limits.depth = 10;
    const SearchResult result = search_position(board, limits);
    total_nodes += result.nodes;
  }

  auto finish = std::chrono::steady_clock::now();
  const double elapsed_ms =
      std::chrono::duration<double, std::milli>(finish - start).count();
  const double nps = elapsed_ms > 0.0 ? (1000.0 * static_cast<double>(total_nodes) / elapsed_ms) : 0.0;

  std::cout << "info string bench positions " << kBenchFens.size() << " nodes " << total_nodes
            << " time " << static_cast<std::uint64_t>(elapsed_ms)
            << " nps " << static_cast<std::uint64_t>(nps) << "\n";
  std::cout << "bestmove 0000\n";
  std::cout.flush();
}

}  // namespace sirio::pyrrhic

