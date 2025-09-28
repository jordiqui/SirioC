#include "engine/search/search.hpp"
#include "engine/search/see.hpp"

#include "engine/core/board.hpp"
#include "engine/eval/eval.hpp"
#include "engine/eval/nnue/evaluator.hpp"
#include "engine/syzygy/syzygy.hpp"
#include "tbprobe.h"
#include "engine/util/time.hpp"
#include "nodes.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <mutex>
#include <sstream>
#include <thread>
#include <atomic>
#include <vector>
#include <optional>
#include <utility>

namespace engine {
namespace {

constexpr int kInfiniteScore = 32000;
constexpr int kMateValue = 30000;
constexpr int kMateThreshold = 29000;
constexpr int kMaxPly = 128;

enum BoundFlag : int { TT_EXACT = 0, TT_LOWER = 1, TT_UPPER = 2 };

int piece_value(char piece) {
    switch (std::tolower(static_cast<unsigned char>(piece))) {
    case 'p': return 100;
    case 'n': return 320;
    case 'b': return 330;
    case 'r': return 500;
    case 'q': return 900;
    default: return 0;
    }
}

constexpr uint64_t bit(int sq) { return 1ULL << sq; }

constexpr std::array<std::pair<int, int>, 4> kBishopDirs{{{1, 1}, {-1, 1},
                                                          {1, -1}, {-1, -1}}};
constexpr std::array<std::pair<int, int>, 4> kRookDirs{{{1, 0}, {-1, 0},
                                                        {0, 1}, {0, -1}}};

inline bool on_board(int file, int rank) {
    return file >= 0 && file < 8 && rank >= 0 && rank < 8;
}

uint64_t knight_attacks(int sq) {
    static const std::array<std::pair<int, int>, 8> kOffsets{{{1, 2},  {2, 1},
                                                              {2, -1}, {1, -2},
                                                              {-1, -2}, {-2, -1},
                                                              {-2, 1},  {-1, 2}}};
    static std::array<uint64_t, 64> table = [] {
        std::array<uint64_t, 64> out{};
        for (int sq = 0; sq < 64; ++sq) {
            int file = sq % 8;
            int rank = sq / 8;
            uint64_t mask = 0ULL;
            for (auto [df, dr] : kOffsets) {
                int nf = file + df;
                int nr = rank + dr;
                if (on_board(nf, nr)) mask |= bit(nr * 8 + nf);
            }
            out[static_cast<size_t>(sq)] = mask;
        }
        return out;
    }();
    return table[static_cast<size_t>(sq)];
}

uint64_t king_attacks(int sq) {
    static const std::array<std::pair<int, int>, 8> kOffsets{{{1, 0},  {1, 1},
                                                              {0, 1},  {-1, 1},
                                                              {-1, 0}, {-1, -1},
                                                              {0, -1}, {1, -1}}};
    static std::array<uint64_t, 64> table = [] {
        std::array<uint64_t, 64> out{};
        for (int sq = 0; sq < 64; ++sq) {
            int file = sq % 8;
            int rank = sq / 8;
            uint64_t mask = 0ULL;
            for (auto [df, dr] : kOffsets) {
                int nf = file + df;
                int nr = rank + dr;
                if (on_board(nf, nr)) mask |= bit(nr * 8 + nf);
            }
            out[static_cast<size_t>(sq)] = mask;
        }
        return out;
    }();
    return table[static_cast<size_t>(sq)];
}

uint64_t sliding_attacks(int sq, uint64_t occ,
                         const std::array<std::pair<int, int>, 4>& dirs) {
    uint64_t attacks = 0ULL;
    int file = sq % 8;
    int rank = sq / 8;
    for (auto [df, dr] : dirs) {
        int nf = file + df;
        int nr = rank + dr;
        while (on_board(nf, nr)) {
            int nsq = nr * 8 + nf;
            attacks |= bit(nsq);
            if (occ & bit(nsq)) break;
            nf += df;
            nr += dr;
        }
    }
    return attacks;
}

uint64_t bishop_attacks(int sq, uint64_t occ) {
    return sliding_attacks(sq, occ, kBishopDirs);
}

uint64_t rook_attacks(int sq, uint64_t occ) {
    return sliding_attacks(sq, occ, kRookDirs);
}

int pop_lsb(uint64_t& bb) {
    uint64_t lsb = bb & -bb;
    int sq = std::countr_zero(lsb);
    bb &= bb - 1;
    return sq;
}

int piece_type_from_char(char piece) {
    switch (std::tolower(static_cast<unsigned char>(piece))) {
    case 'p': return 0;
    case 'n': return 1;
    case 'b': return 2;
    case 'r': return 3;
    case 'q': return 4;
    case 'k': return 5;
    default: return -1;
    }
}

char piece_char_from_type(int color, int type) {
    static const std::array<std::array<char, 6>, 2> chars{{{{'P', 'N', 'B', 'R', 'Q', 'K'}},
                                                           {{'p', 'n', 'b', 'r', 'q', 'k'}}}};
    if (color < 0 || color > 1 || type < 0 || type >= 6) return '.';
    return chars[static_cast<size_t>(color)][static_cast<size_t>(type)];
}

char promotion_piece_char(int promo, bool white) {
    switch (promo) {
    case 1: return white ? 'N' : 'n';
    case 2: return white ? 'B' : 'b';
    case 3: return white ? 'R' : 'r';
    case 4: return white ? 'Q' : 'q';
    default: return white ? 'P' : 'p';
    }
}

struct PinInfo {
    uint64_t pinned_mask = 0ULL;
    std::array<uint64_t, 64> rays{};
};

PinInfo compute_pin_info(int color,
                         const std::array<std::array<uint64_t, 6>, 2>& pieces,
                         uint64_t occ) {
    PinInfo info;
    uint64_t king_bb = pieces[static_cast<size_t>(color)][5];
    if (!king_bb) return info;
    int king_sq = std::countr_zero(king_bb);
    int king_file = king_sq % 8;
    int king_rank = king_sq / 8;
    int enemy = color ^ 1;
    uint64_t friendly = 0ULL;
    for (int t = 0; t < 6; ++t) {
        friendly |= pieces[static_cast<size_t>(color)][static_cast<size_t>(t)];
    }

    auto handle_dir = [&](const std::pair<int, int>& dir, uint64_t enemy_sliders) {
        int nf = king_file + dir.first;
        int nr = king_rank + dir.second;
        int pinned_sq = -1;
        while (on_board(nf, nr)) {
            int sq = nr * 8 + nf;
            uint64_t mask = bit(sq);
            if (occ & mask) {
                if (mask & king_bb) {
                    break;
                }
                if (pinned_sq == -1 && (mask & friendly)) {
                    pinned_sq = sq;
                } else {
                    if (mask & enemy_sliders && pinned_sq != -1) {
                        info.pinned_mask |= bit(pinned_sq);
                        int pf = king_file + dir.first;
                        int pr = king_rank + dir.second;
                        uint64_t ray = 0ULL;
                        while (on_board(pf, pr)) {
                            int psq = pr * 8 + pf;
                            ray |= bit(psq);
                            if (psq == sq) break;
                            pf += dir.first;
                            pr += dir.second;
                        }
                        info.rays[static_cast<size_t>(pinned_sq)] = ray;
                    }
                    break;
                }
            }
            nf += dir.first;
            nr += dir.second;
        }
    };

    uint64_t enemy_bishops = pieces[static_cast<size_t>(enemy)][2] |
                             pieces[static_cast<size_t>(enemy)][4];
    uint64_t enemy_rooks = pieces[static_cast<size_t>(enemy)][3] |
                           pieces[static_cast<size_t>(enemy)][4];

    for (const auto& dir : kBishopDirs) {
        handle_dir(dir, enemy_bishops);
    }
    for (const auto& dir : kRookDirs) {
        handle_dir(dir, enemy_rooks);
    }

    return info;
}

std::array<uint64_t, 2>
attackers_to(int sq, uint64_t occ,
             const std::array<std::array<uint64_t, 6>, 2>& pieces) {
    std::array<uint64_t, 2> attackers{0ULL, 0ULL};
    int file = sq % 8;
    int rank = sq / 8;

    if (file > 0 && rank > 0) {
        int from = sq - 9;
        if (pieces[0][0] & bit(from)) attackers[0] |= bit(from);
    }
    if (file < 7 && rank > 0) {
        int from = sq - 7;
        if (pieces[0][0] & bit(from)) attackers[0] |= bit(from);
    }
    if (file > 0 && rank < 7) {
        int from = sq + 7;
        if (pieces[1][0] & bit(from)) attackers[1] |= bit(from);
    }
    if (file < 7 && rank < 7) {
        int from = sq + 9;
        if (pieces[1][0] & bit(from)) attackers[1] |= bit(from);
    }

    uint64_t knight_mask = knight_attacks(sq);
    attackers[0] |= knight_mask & pieces[0][1];
    attackers[1] |= knight_mask & pieces[1][1];

    uint64_t bishop_mask = bishop_attacks(sq, occ);
    attackers[0] |= bishop_mask & (pieces[0][2] | pieces[0][4]);
    attackers[1] |= bishop_mask & (pieces[1][2] | pieces[1][4]);

    uint64_t rook_mask = rook_attacks(sq, occ);
    attackers[0] |= rook_mask & (pieces[0][3] | pieces[0][4]);
    attackers[1] |= rook_mask & (pieces[1][3] | pieces[1][4]);

    uint64_t king_mask = king_attacks(sq);
    attackers[0] |= king_mask & pieces[0][5];
    attackers[1] |= king_mask & pieces[1][5];

    return attackers;
}

constexpr int kHistoryMax = 1'000'000;
constexpr int kHistoryMin = -kHistoryMax;

constexpr int history_index(Move move) {
    return move_from(move) * 64 + move_to(move);
}

constexpr std::array<int, 4> kFutilityMargins{0, 120, 200, 320};
constexpr int kAspirationWindow = 25;
constexpr int kNullMoveBaseReduction = 2;
constexpr int kMaxNullMoveReduction = 3;
constexpr int kNullMoveDepthDivisor = 4;
constexpr int kReverseFutilityMargin = 200;
constexpr int kRazoringMargin = 400;
constexpr int kProbCutMargin = 200;
constexpr int kSingularMarginScale = 32;
constexpr int kSingularReductionBase = 2;
constexpr std::array<int, 7> kLateMovePruningLimits{0, 3, 6, 10, 14, 18, 24};
constexpr std::array<int, 7> kEnhancedBetaMargins{0, 90, 140, 200, 260, 340, 420};

bool has_non_pawn_material(const Board& board, bool white) {
    const auto& bb = board.piece_bitboards();
    if (white) {
        for (int idx = Board::WHITE_KNIGHT; idx <= Board::WHITE_QUEEN; ++idx) {
            if (bb[static_cast<size_t>(idx)] != 0ULL) return true;
        }
    } else {
        for (int idx = Board::BLACK_KNIGHT; idx <= Board::BLACK_QUEEN; ++idx) {
            if (bb[static_cast<size_t>(idx)] != 0ULL) return true;
        }
    }
    return false;
}

int static_exchange_eval_impl(const Board& board, Move move) {
    int from = move_from(move);
    int to = move_to(move);
    char moving_piece = board.piece_on(from);
    bool en_passant = move_is_enpassant(move);
    bool is_capture = move_is_capture(move) || en_passant;
    if (!is_capture) return 0;

    char captured = board.piece_on(to);
    bool moving_white = std::isupper(static_cast<unsigned char>(moving_piece));
    if (en_passant) {
        captured = moving_white ? 'p' : 'P';
    }
    if (captured == '.') return 0;

    int promo = move_promo(move);
    char promoted_piece = moving_piece;
    if (promo != 0 && std::tolower(static_cast<unsigned char>(moving_piece)) == 'p') {
        promoted_piece = promotion_piece_char(promo, moving_white);
    }

    constexpr int WHITE = 0;
    constexpr int BLACK = 1;

    std::array<std::array<uint64_t, 6>, 2> pieces{};
    const auto& bb = board.piece_bitboards();
    pieces[WHITE][0] = bb[Board::WHITE_PAWN];
    pieces[WHITE][1] = bb[Board::WHITE_KNIGHT];
    pieces[WHITE][2] = bb[Board::WHITE_BISHOP];
    pieces[WHITE][3] = bb[Board::WHITE_ROOK];
    pieces[WHITE][4] = bb[Board::WHITE_QUEEN];
    pieces[WHITE][5] = bb[Board::WHITE_KING];
    pieces[BLACK][0] = bb[Board::BLACK_PAWN];
    pieces[BLACK][1] = bb[Board::BLACK_KNIGHT];
    pieces[BLACK][2] = bb[Board::BLACK_BISHOP];
    pieces[BLACK][3] = bb[Board::BLACK_ROOK];
    pieces[BLACK][4] = bb[Board::BLACK_QUEEN];
    pieces[BLACK][5] = bb[Board::BLACK_KING];

    std::array<uint64_t, 2> occ_color{board.occupancy()[Board::OCC_WHITE],
                                      board.occupancy()[Board::OCC_BLACK]};
    uint64_t occ_all = board.occupancy()[Board::OCC_BOTH];

    std::array<int, 32> gains{};
    gains[0] = piece_value(captured);
    int depth = 0;

    int side = moving_white ? WHITE : BLACK;
    int from_piece_type = piece_type_from_char(moving_piece);
    if (from_piece_type < 0) return gains[0];

    char target_piece = captured;

    auto apply_capture = [&](int color, int from_sq, int piece_type_from,
                             char arriving_piece, char& captured_piece,
                             bool en_passant_capture) {
        gains[++depth] = piece_value(arriving_piece) - gains[depth - 1];
        bool continue_sequence = std::max(-gains[depth - 1], gains[depth]) >= 0;

        int enemy = color ^ 1;
        uint64_t from_bb = bit(from_sq);
        uint64_t to_bb = bit(to);

        if (en_passant_capture) {
            int ep_sq = to + (color == WHITE ? -8 : 8);
            uint64_t ep_bb = bit(ep_sq);
            pieces[static_cast<size_t>(enemy)][0] &= ~ep_bb;
            occ_color[static_cast<size_t>(enemy)] &= ~ep_bb;
        } else {
            int captured_type = piece_type_from_char(captured_piece);
            if (captured_type >= 0) {
                pieces[static_cast<size_t>(enemy)][static_cast<size_t>(captured_type)] &=
                    ~to_bb;
            }
            occ_color[static_cast<size_t>(enemy)] &= ~to_bb;
        }

        pieces[static_cast<size_t>(color)][static_cast<size_t>(piece_type_from)] &=
            ~from_bb;

        int new_type = piece_type_from_char(arriving_piece);
        if (new_type < 0) new_type = piece_type_from;
        pieces[static_cast<size_t>(color)][static_cast<size_t>(new_type)] |= to_bb;

        occ_color[static_cast<size_t>(color)] &= ~from_bb;
        occ_color[static_cast<size_t>(color)] |= to_bb;
        occ_all = occ_color[WHITE] | occ_color[BLACK];

        captured_piece = arriving_piece;
        return continue_sequence;
    };

    bool continue_sequence =
        apply_capture(side, from, from_piece_type, promoted_piece, target_piece,
                      en_passant);
    if (!continue_sequence) {
        while (depth > 0) {
            gains[depth - 1] = -std::max(-gains[depth - 1], gains[depth]);
            --depth;
        }
        return gains[0];
    }

    en_passant = false;
    side ^= 1;

    while (true) {
        auto attackers = attackers_to(to, occ_all, pieces);
        PinInfo pins = compute_pin_info(side, pieces, occ_all);
        uint64_t mask = attackers[static_cast<size_t>(side)];

        auto select_attacker = [&](uint64_t candidates) -> std::pair<int, int> {
            static constexpr std::array<int, 6> kOrder{0, 1, 2, 3, 4, 5};
            for (int type : kOrder) {
                uint64_t bb_mask =
                    pieces[static_cast<size_t>(side)][static_cast<size_t>(type)] &
                    candidates;
                while (bb_mask) {
                    int sq = pop_lsb(bb_mask);
                    uint64_t sq_bb = bit(sq);
                    if ((pins.pinned_mask & sq_bb) &&
                        !(pins.rays[static_cast<size_t>(sq)] & bit(to))) {
                        continue;
                    }
                    return {sq, type};
                }
            }
            return {-1, -1};
        };

        auto [attacker_sq, attacker_type] = select_attacker(mask);
        if (attacker_sq == -1) break;

        char attacker_piece = piece_char_from_type(side, attacker_type);
        continue_sequence = apply_capture(side, attacker_sq, attacker_type,
                                          attacker_piece, target_piece, false);
        if (!continue_sequence) break;
        side ^= 1;
    }

    while (depth > 0) {
        gains[depth - 1] = -std::max(-gains[depth - 1], gains[depth]);
        --depth;
    }
    return gains[0];
}

std::string wdl_to_string(syzygy::WdlOutcome outcome) {
    switch (outcome) {
    case syzygy::WdlOutcome::Win: return "win";
    case syzygy::WdlOutcome::CursedWin: return "cursed-win";
    case syzygy::WdlOutcome::Draw: return "draw";
    case syzygy::WdlOutcome::BlessedLoss: return "blessed-loss";
    case syzygy::WdlOutcome::Loss: return "loss";
    }
    return "unknown";
}

int tablebase_score(const syzygy::TB::ProbeResult& probe,
                    const syzygy::TBConfig& config) {
    auto mate_score = [](bool winning, int distance) {
        int d = std::max(1, distance);
        int capped = std::min(kMateValue - 1, d);
        int score = kMateValue - capped;
        return winning ? score : -score;
    };

    switch (probe.wdl) {
    case syzygy::WdlOutcome::Win: {
        int dist = probe.dtz ? std::max(1, std::abs(*probe.dtz)) : 1;
        return mate_score(true, dist);
    }
    case syzygy::WdlOutcome::Loss: {
        int dist = probe.dtz ? std::max(1, std::abs(*probe.dtz)) : 1;
        return mate_score(false, dist);
    }
    case syzygy::WdlOutcome::Draw:
        return 0;
    case syzygy::WdlOutcome::CursedWin:
        if (config.use_rule50) return 0;
        return mate_score(true, probe.dtz ? std::max(1, std::abs(*probe.dtz)) : 1);
    case syzygy::WdlOutcome::BlessedLoss:
        if (config.use_rule50) return 0;
        return mate_score(false, probe.dtz ? std::max(1, std::abs(*probe.dtz)) : 1);
    }
    return 0;
}

} // namespace

int static_exchange_eval(const Board& board, Move move) {
    return static_exchange_eval_impl(board, move);
}

Search::ThreadData::ThreadData() { reset(); }

void Search::ThreadData::reset() {
    killers.assign(kMaxPly, {MOVE_NONE, MOVE_NONE});
    history.fill(0);
    countermoves.fill(MOVE_NONE);
    quiescence_captures.clear();
    quiescence_checks.clear();
}

void Search::AdaptiveTuning::reset() {
    futility_margins_ = kFutilityMargins;
    late_move_limits_ = kLateMovePruningLimits;
    beta_margins_ = kEnhancedBetaMargins;
    probcut_margin_ = kProbCutMargin;
    singular_margin_scale_ = kSingularMarginScale;
    singular_reduction_base_ = kSingularReductionBase;
    null_move_base_reduction_ = kNullMoveBaseReduction;
    null_move_max_reduction_ = kMaxNullMoveReduction;
    null_move_depth_divisor_ = kNullMoveDepthDivisor;
    reverse_futility_margin_ = kReverseFutilityMargin;
    razoring_margin_ = kRazoringMargin;
    lmr_scale_ = 1.0;
    threads_ = 1;
    target_time_ms_ = -1;
    has_baseline_speed_ = false;
    baseline_nodes_per_ms_ = 1.0;
    speed_ema_ = 1.0;
    iteration_nodes_start_ = 0;
    iteration_start_ = {};
}

void Search::AdaptiveTuning::prepare(int threads, int64_t target_time_ms) {
    threads_ = std::max(1, threads);
    target_time_ms_ = target_time_ms;
    has_baseline_speed_ = false;
    baseline_nodes_per_ms_ = 1.0;
    speed_ema_ = 1.0;
    apply_scaling();
}

void Search::AdaptiveTuning::begin_iteration(
    uint64_t nodes, std::chrono::steady_clock::time_point start_time) {
    iteration_nodes_start_ = nodes;
    iteration_start_ = start_time;
}

void Search::AdaptiveTuning::end_iteration(
    uint64_t nodes, std::chrono::steady_clock::time_point end_time) {
    if (iteration_nodes_start_ > nodes) {
        iteration_nodes_start_ = nodes;
    }
    auto elapsed = std::chrono::duration<double, std::milli>(end_time - iteration_start_).count();
    if (elapsed <= 0.0) {
        return;
    }
    double explored = static_cast<double>(nodes - iteration_nodes_start_);
    if (explored <= 0.0) {
        return;
    }
    double nodes_per_ms = explored / elapsed;
    if (!has_baseline_speed_) {
        baseline_nodes_per_ms_ = std::max(1e-6, nodes_per_ms);
        has_baseline_speed_ = true;
        speed_ema_ = 1.0;
    } else {
        double ratio = nodes_per_ms / std::max(1e-6, baseline_nodes_per_ms_);
        speed_ema_ = 0.85 * speed_ema_ + 0.15 * std::clamp(ratio, 0.2, 5.0);
    }
    apply_scaling();
}

int Search::AdaptiveTuning::futility_margin(int depth) const {
    size_t idx = static_cast<size_t>(std::min(depth, static_cast<int>(futility_margins_.size() - 1)));
    return futility_margins_[idx];
}

int Search::AdaptiveTuning::reverse_futility_margin() const { return reverse_futility_margin_; }

int Search::AdaptiveTuning::razoring_margin() const { return razoring_margin_; }

int Search::AdaptiveTuning::late_move_limit(int depth, int move_overhead_ms, int history_score,
                                            int move_count) const {
    size_t idx = static_cast<size_t>(std::min(depth, static_cast<int>(late_move_limits_.size() - 1)));
    int limit = late_move_limits_[idx] + move_overhead_ms / 40;
    if (history_score < 0) {
        --limit;
        if (move_count > limit) {
            --limit;
        }
    }
    return std::max(0, limit);
}

int Search::AdaptiveTuning::beta_margin(int depth) const {
    size_t idx = static_cast<size_t>(std::min(depth, static_cast<int>(beta_margins_.size() - 1)));
    return beta_margins_[idx];
}

int Search::AdaptiveTuning::probcut_margin() const { return probcut_margin_; }

int Search::AdaptiveTuning::singular_margin_scale() const { return singular_margin_scale_; }

int Search::AdaptiveTuning::singular_reduction_base() const { return singular_reduction_base_; }

int Search::AdaptiveTuning::null_move_base_reduction() const { return null_move_base_reduction_; }

int Search::AdaptiveTuning::null_move_max_reduction() const { return null_move_max_reduction_; }

int Search::AdaptiveTuning::null_move_depth_divisor() const { return null_move_depth_divisor_; }

double Search::AdaptiveTuning::lmr_scale() const { return lmr_scale_; }

void Search::AdaptiveTuning::apply_scaling() {
    futility_margins_ = kFutilityMargins;
    late_move_limits_ = kLateMovePruningLimits;
    beta_margins_ = kEnhancedBetaMargins;
    probcut_margin_ = kProbCutMargin;
    singular_margin_scale_ = kSingularMarginScale;
    singular_reduction_base_ = kSingularReductionBase;
    null_move_base_reduction_ = kNullMoveBaseReduction;
    null_move_max_reduction_ = kMaxNullMoveReduction;
    null_move_depth_divisor_ = kNullMoveDepthDivisor;
    reverse_futility_margin_ = kReverseFutilityMargin;
    razoring_margin_ = kRazoringMargin;
    lmr_scale_ = 1.0;

    double hardware_scale = std::clamp(std::sqrt(static_cast<double>(threads_)), 1.0, 2.0);
    double time_scale = 1.0;
    if (target_time_ms_ > 0) {
        time_scale = std::clamp(std::sqrt(static_cast<double>(target_time_ms_) / 1000.0), 0.6, 2.0);
    }
    double speed_scale = std::clamp(speed_ema_, 0.5, 1.8);
    double effective = std::clamp(hardware_scale * time_scale * speed_scale, 0.6, 1.8);
    double margin_scale = std::clamp(1.0 / std::sqrt(effective), 0.7, 1.3);
    double pruning_scale = std::clamp(std::sqrt(effective), 0.8, 1.25);
    double extension_scale = std::clamp(1.0 / pruning_scale, 0.75, 1.25);
    lmr_scale_ = std::clamp(extension_scale, 0.75, 1.25);

    for (size_t i = 0; i < futility_margins_.size(); ++i) {
        futility_margins_[i] = std::max(0, static_cast<int>(std::round(kFutilityMargins[i] * margin_scale)));
    }
    for (size_t i = 0; i < late_move_limits_.size(); ++i) {
        late_move_limits_[i] = std::max(0, static_cast<int>(std::round(kLateMovePruningLimits[i] * pruning_scale)));
    }
    for (size_t i = 0; i < beta_margins_.size(); ++i) {
        beta_margins_[i] = std::max(50, static_cast<int>(std::round(kEnhancedBetaMargins[i] * margin_scale)));
    }

    probcut_margin_ = std::max(80, static_cast<int>(std::round(kProbCutMargin * margin_scale)));
    singular_margin_scale_ = std::max(16, static_cast<int>(std::round(kSingularMarginScale * margin_scale)));
    singular_reduction_base_ = std::clamp(static_cast<int>(std::round(kSingularReductionBase * extension_scale)), 1, 4);
    null_move_base_reduction_ = std::clamp(static_cast<int>(std::round(kNullMoveBaseReduction * extension_scale)), 1, 4);
    null_move_max_reduction_ = std::clamp(static_cast<int>(std::round(kMaxNullMoveReduction * extension_scale)), 2, 5);
    null_move_depth_divisor_ = std::clamp(static_cast<int>(std::round(kNullMoveDepthDivisor / std::clamp(effective, 0.7, 1.6))), 3, 6);
    reverse_futility_margin_ = std::max(80, static_cast<int>(std::round(kReverseFutilityMargin * margin_scale)));
    razoring_margin_ = std::max(200, static_cast<int>(std::round(kRazoringMargin * margin_scale)));
}

Search::Search() : stop_(false) {
    set_hash(16);
    target_time_ms_ = -1;
    nodes_limit_ = -1;
    tuning_.reset();
}

void Search::start_worker_threads(size_t thread_count) {
    stop_worker_threads();
    if (thread_count <= 1) {
        pool_stop_.store(false, std::memory_order_relaxed);
        pending_tasks_.store(0, std::memory_order_relaxed);
        return;
    }
    pool_stop_.store(false, std::memory_order_relaxed);
    pending_tasks_.store(0, std::memory_order_relaxed);
    worker_threads_.reserve(thread_count - 1);
    for (size_t i = 1; i < thread_count; ++i) {
        worker_threads_.emplace_back(&Search::worker_loop, this, i);
    }
}

void Search::stop_worker_threads() {
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        pool_stop_.store(true, std::memory_order_relaxed);
    }
    task_cv_.notify_all();
    for (auto& th : worker_threads_) {
        if (th.joinable()) th.join();
    }
    worker_threads_.clear();
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        task_queue_.clear();
    }
    pending_tasks_.store(0, std::memory_order_relaxed);
    pool_stop_.store(false, std::memory_order_relaxed);
}

void Search::submit_task(std::function<void(ThreadData&)> task) {
    if (worker_threads_.empty()) { return; }
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        task_queue_.emplace_back(std::move(task));
        pending_tasks_.fetch_add(1, std::memory_order_relaxed);
    }
    task_cv_.notify_one();
}

bool Search::run_available_task(Search::ThreadData& main_td) {
    std::function<void(Search::ThreadData&)> task;
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        if (task_queue_.empty()) return false;
        task = std::move(task_queue_.front());
        task_queue_.pop_front();
    }
    task(main_td);
    if (pending_tasks_.fetch_sub(1, std::memory_order_relaxed) == 1) {
        task_done_cv_.notify_all();
    }
    return true;
}

void Search::wait_for_all_tasks(Search::ThreadData& main_td) {
    while (true) {
        if (pending_tasks_.load(std::memory_order_acquire) == 0) break;
        if (!run_available_task(main_td)) {
            std::unique_lock<std::mutex> lock(task_mutex_);
            task_done_cv_.wait(lock, [&] {
                return pending_tasks_.load(std::memory_order_acquire) == 0 || !task_queue_.empty() ||
                       pool_stop_.load(std::memory_order_relaxed);
            });
            if (pool_stop_.load(std::memory_order_relaxed)) break;
        }
    }
}

void Search::worker_loop(size_t index) {
    Search::ThreadData& td = thread_data_pool_[index];
    while (true) {
        std::function<void(Search::ThreadData&)> task;
        {
            std::unique_lock<std::mutex> lock(task_mutex_);
            task_cv_.wait(lock, [&] {
                return pool_stop_.load(std::memory_order_relaxed) || !task_queue_.empty();
            });
            if (pool_stop_.load(std::memory_order_relaxed) && task_queue_.empty()) { return; }
            task = std::move(task_queue_.front());
            task_queue_.pop_front();
        }
        task(td);
        if (pending_tasks_.fetch_sub(1, std::memory_order_relaxed) == 1) {
            task_done_cv_.notify_all();
        }
    }
}

void Search::set_info_callback(std::function<void(const Info&)> cb) {
    info_callback_ = std::move(cb);
}

void Search::set_threads(int threads) { threads_ = std::max(1, threads); }

void Search::set_hash(int megabytes) { tt_.resize(static_cast<size_t>(std::max(1, megabytes))); }

void Search::stop() { stop_.store(true, std::memory_order_relaxed); }

void Search::set_syzygy_config(syzygy::TBConfig config) {
    syzygy_config_ = std::move(config);
    if (!syzygy_config_.enabled || syzygy_config_.path.empty()) {
        syzygy::shutdown();
    } else {
        syzygy::configure(syzygy_config_);
    }
}

void Search::set_numa_offset(int offset) { numa_offset_ = offset; }

void Search::set_ponder(bool enable) { ponder_ = enable; }

void Search::set_multi_pv(int multi_pv) { multi_pv_ = std::max(1, multi_pv); }

void Search::set_move_overhead(int overhead_ms) { move_overhead_ms_ = std::max(0, overhead_ms); }

void Search::set_time_config(time::TimeConfig config) { time_config_ = std::move(config); }

void Search::set_eval_file(std::string path) { eval_file_ = std::move(path); }

void Search::set_eval_file_small(std::string path) { eval_file_small_ = std::move(path); }

void Search::set_nnue_evaluator(const nnue::Evaluator* evaluator) { nnue_eval_ = evaluator; }

void Search::set_use_nnue(bool enable) { use_nnue_eval_ = enable; }

void Search::set_show_wdl(bool enable) { show_wdl_ = enable; }

void Search::set_chess960(bool enable) { chess960_ = enable; }

void Search::set_contempt(int value) { contempt_ = value; }

Search::Result Search::find_bestmove(Board& board, const Limits& lim) {
    stop_.store(false, std::memory_order_relaxed);
    return search_position(board, lim);
}

Search::Result Search::search_position(Board& board, const Limits& lim) {
    Result result;
    auto start = std::chrono::steady_clock::now();
    search_start_ = start;

    size_t thread_count = static_cast<size_t>(std::max(1, threads_));
    bool thread_count_changed = thread_data_thread_count_ != thread_count;
    uint64_t position_key = board.zobrist_key();
    bool new_position = !thread_data_initialized_ || thread_data_position_key_ != position_key;
    thread_data_pool_.resize(thread_count);
    for (size_t i = 0; i < thread_count; ++i) {
        thread_data_pool_[i].id = i;
    }
    if (thread_count_changed || new_position) {
        for (auto& td : thread_data_pool_) {
            td.reset();
        }
    }
    init_nodes(thread_count);
    publish_nodes_relaxed();
    tt_.new_search();
    thread_data_thread_count_ = thread_count;
    thread_data_position_key_ = position_key;
    thread_data_initialized_ = true;

    start_worker_threads(thread_count);
    struct ThreadPoolGuard {
        Search& search;
        ~ThreadPoolGuard() { search.stop_worker_threads(); }
    };
    [[maybe_unused]] ThreadPoolGuard pool_guard{*this};

    auto alloc = time::compute_allocation(lim, board.white_to_move(), move_overhead_ms_,
                                          board.fullmove_number(), time_config_);
    target_time_ms_ = alloc.optimal_ms;
    if (alloc.maximum_ms > 0) {
        deadline_ = start + std::chrono::milliseconds(alloc.maximum_ms);
    } else {
        deadline_.reset();
    }
    nodes_limit_ = lim.nodes >= 0 ? lim.nodes : -1;

    if (alloc.optimal_ms > 0) {
        std::ostringstream oss;
        oss << "info string tm alloc optimal " << alloc.optimal_ms << "ms max " << alloc.maximum_ms
            << "ms movesToGo " << alloc.moves_to_go;
        if (alloc.base_ms > 0) oss << " base " << alloc.base_ms << "ms";
        if (alloc.time_left_ms >= 0) oss << " tl " << alloc.time_left_ms << "ms";
        if (alloc.increment_ms > 0) oss << " inc " << alloc.increment_ms << "ms";
        if (alloc.usable_increment_ms > 0) oss << " incReserve " << alloc.usable_increment_ms << "ms";
        if (alloc.severe_time_pressure) oss << " severe";
        if (alloc.panic_mode) oss << " panic";
        std::cout << oss.str() << '\n' << std::flush;
    }

    tuning_.prepare(threads_, target_time_ms_);

    auto legal = board.generate_legal_moves();
    if (legal.empty()) {
        result.bestmove = MOVE_NONE;
        result.depth = 0;
        result.nodes = 0;
        result.time_ms = 0;
        deadline_.reset();
        target_time_ms_ = -1;
        nodes_limit_ = -1;
        return result;
    }

    std::vector<Move> root_moves = legal;
    Move best_move = root_moves.front();
    int best_score = 0;
    result.bestmove = best_move;
    result.depth = 0;
    result.score = best_score;
    result.is_mate = false;

    int depth_limit = lim.depth > 0 ? lim.depth : 64;
    depth_limit = std::min(depth_limit, 64);

    int aspiration_delta = kAspirationWindow;
    int fail_high_streak = 0;
    int fail_low_streak = 0;

    bool attempted_root_tb = false;

    for (int depth = 1; depth <= depth_limit; ++depth) {
        if (stop_.load(std::memory_order_relaxed)) break;

        if (!attempted_root_tb && syzygy_config_.enabled && !syzygy_config_.path.empty() &&
            depth >= syzygy_config_.probe_depth) {
            int tb_limit = syzygy_config_.probe_limit;
            if (tb_limit < 0 || syzygy::TB::pieceCount(board) <= tb_limit) {
                if (auto tb = syzygy::TB::probePosition(board, syzygy_config_, true)) {
                    int tb_score = tablebase_score(*tb, syzygy_config_);
                    result.bestmove = tb->best_move.value_or(MOVE_NONE);
                    result.depth = depth;
                    result.score = tb_score;
                    result.is_mate = std::abs(tb_score) >= kMateThreshold;
                    result.nodes = publish_nodes_relaxed();
                    result.time_ms = static_cast<int>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start)
                            .count());
                    result.pv.clear();
                    if (tb->best_move) {
                        result.pv.push_back(*tb->best_move);
                    }

                    std::ostringstream oss;
                    oss << "info string Syzygy hit wdl " << wdl_to_string(tb->wdl);
                    if (tb->dtz) {
                        oss << " dtz " << *tb->dtz;
                    }
                    if (tb->best_move) {
                        oss << " move " << board.move_to_uci(*tb->best_move);
                    }
                    std::cout << oss.str() << '\n' << std::flush;

                    deadline_.reset();
                    target_time_ms_ = -1;
                    nodes_limit_ = -1;
                    return result;
                }
            }
            attempted_root_tb = true;
        }

        tuning_.begin_iteration(total_nodes_relaxed(), std::chrono::steady_clock::now());

        int alpha_window = -kInfiniteScore;
        int beta_window = kInfiniteScore;
        int local_delta = std::max(kAspirationWindow,
                                   aspiration_delta + fail_high_streak * 5 +
                                       fail_low_streak * 5);
        if (depth > 1 && result.depth > 0) {
            alpha_window = std::max(-kInfiniteScore, best_score - local_delta);
            beta_window = std::min(kInfiniteScore, best_score + local_delta);
        }

        std::vector<std::pair<Move, int>> best_scores;

        while (!stop_.load(std::memory_order_relaxed)) {
            int search_alpha = alpha_window;
            int search_beta = beta_window;
            std::vector<std::optional<std::pair<Move, int>>> scores(root_moves.size());
            std::atomic<size_t> next_index{0};
            std::atomic<size_t> completed{0};

            auto worker = [&](ThreadData& td) {
                Board local_board = board;
                while (!stop_.load(std::memory_order_relaxed)) {
                    size_t idx = next_index.fetch_add(1, std::memory_order_relaxed);
                    if (idx >= root_moves.size()) break;
                    Move move = root_moves[idx];
                    Board::State state;
                    local_board.apply_move(move, state);
                    int score = -negamax(local_board, depth - 1, -search_beta, -search_alpha, true, 1, td, move, false);
                    local_board.undo_move(state);
                    scores[idx] = std::make_pair(move, score);
                    completed.fetch_add(1, std::memory_order_relaxed);
                }
            };

            if (thread_count > 1) {
                for (size_t t = 1; t < thread_count; ++t) {
                    submit_task(worker);
                }
            }
            worker(thread_data_pool_[0]);
            if (thread_count > 1) {
                wait_for_all_tasks(thread_data_pool_[0]);
            }

            if (stop_.load(std::memory_order_relaxed)) break;

            size_t finished = std::min(completed.load(std::memory_order_relaxed), scores.size());
            std::vector<std::pair<Move, int>> filtered_scores;
            filtered_scores.reserve(finished);
            for (auto& entry : scores) {
                if (entry && entry->first != MOVE_NONE) {
                    filtered_scores.push_back(*entry);
                }
            }

            if (filtered_scores.empty()) {
                break;
            }

            std::sort(filtered_scores.begin(), filtered_scores.end(), [](const auto& lhs, const auto& rhs) {
                return lhs.second > rhs.second;
            });

            if (!filtered_scores.empty()) {
                best_scores = filtered_scores;
                best_move = filtered_scores.front().first;
                best_score = filtered_scores.front().second;
                bool failed_low = best_score <= search_alpha;
                bool failed_high = best_score >= search_beta;

                if (failed_low) {
                    fail_low_streak++;
                    fail_high_streak = 0;
                    local_delta = std::min(400, local_delta * 2 + 5);
                    alpha_window = std::max(-kInfiniteScore, best_score - local_delta);
                    beta_window = search_beta;
                    if (alpha_window <= -kInfiniteScore + 1 || search_alpha == -kInfiniteScore) {
                        alpha_window = -kInfiniteScore;
                        beta_window = search_beta;
                        break;
                    }
                    continue;
                }
                if (failed_high) {
                    fail_high_streak++;
                    fail_low_streak = 0;
                    local_delta = std::min(400, local_delta * 2 + 5);
                    beta_window = std::min(kInfiniteScore, best_score + local_delta);
                    alpha_window = search_alpha;
                    if (beta_window >= kInfiniteScore - 1 || search_beta == kInfiniteScore) {
                        beta_window = kInfiniteScore;
                        alpha_window = search_alpha;
                        break;
                    }
                    continue;
                }

                result.bestmove = best_move;
                result.depth = depth;
                result.score = best_score;
                result.is_mate = std::abs(best_score) >= kMateThreshold;

                aspiration_delta = std::max(kAspirationWindow / 2, local_delta / 2);
                fail_high_streak = 0;
                fail_low_streak = 0;

                if (info_callback_) {
                    Info info;
                    info.depth = depth;
                    info.score = best_score;
                    info.nodes = publish_nodes_relaxed();
                    info.time_ms = static_cast<int>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start)
                            .count());
                    info.pv = extract_pv(board, best_move);
                    info_callback_(info);
                }
                break;
            }
        }

        if (!best_scores.empty()) {
            root_moves.clear();
            for (const auto& [move, score] : best_scores) {
                root_moves.push_back(move);
            }
        }

        auto now = std::chrono::steady_clock::now();
        tuning_.end_iteration(total_nodes_relaxed(), now);
        if (deadline_ && now >= *deadline_) {
            stop_.store(true, std::memory_order_relaxed);
            break;
        }
        if (target_time_ms_ > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - search_start_).count();
            if (elapsed >= target_time_ms_) {
                break;
            }
        }
    }

    result.nodes = publish_nodes_relaxed();
    result.time_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start)
            .count());
    result.pv = extract_pv(board, result.bestmove);

    deadline_.reset();
    target_time_ms_ = -1;
    nodes_limit_ = -1;
    return result;
}

int Search::negamax(Board& board, int depth, int alpha, int beta, bool pv_node, int ply,
                    ThreadData& thread_data, Move prev_move, bool in_iid) {
    if (stop_.load(std::memory_order_relaxed)) return 0;
    if (deadline_ && std::chrono::steady_clock::now() >= *deadline_) {
        stop_.store(true, std::memory_order_relaxed);
        return evaluate(board);
    }

    uint64_t visited = inc_node(thread_data.id);
    if (nodes_limit_ >= 0 && visited >= static_cast<uint64_t>(nodes_limit_)) {
        stop_.store(true, std::memory_order_relaxed);
        return evaluate(board);
    }

    bool in_check = board.side_to_move_in_check();
    if (in_check) ++depth;

    if (ply >= static_cast<int>(thread_data.killers.size())) {
        thread_data.killers.resize(ply + 1, {MOVE_NONE, MOVE_NONE});
    }

    if (depth <= 0) { return quiescence(board, alpha, beta, ply, thread_data); }

    if (auto tb = probe_syzygy(board, depth, false)) return *tb;

    int static_eval = evaluate(board);

    Move tt_move = MOVE_NONE;
    int tt_score = static_eval;
    int tt_depth = -1;
    int tt_flag = TT_EXACT;
    int tt_eval = static_eval;
    if (probe_tt(board, depth, alpha, beta, tt_move, tt_score, ply, tt_depth, tt_flag, tt_eval)) {
        return tt_score;
    }
    static_eval = tt_eval;

    if (!pv_node && depth <= 3 && !in_check) {
        int margin = static_eval - tuning_.reverse_futility_margin() * depth;
        if (margin >= beta) return margin;
    }

    if (!pv_node && depth <= 3 && !in_check &&
        static_eval + tuning_.razoring_margin() <= alpha) {
        int razor = quiescence(board, alpha - 1, alpha, ply, thread_data);
        if (razor <= alpha) return razor;
    }

    if (!pv_node && depth >= 2 && !in_check && static_eval >= beta &&
        has_non_pawn_material(board, board.white_to_move())) {
        Board::State null_state;
        board.apply_null_move(null_state);
        int reduction = tuning_.null_move_base_reduction();
        int extra = depth / std::max(1, tuning_.null_move_depth_divisor());
        reduction += std::min(tuning_.null_move_max_reduction(), extra);
        int score = -negamax(board, depth - 1 - reduction, -beta, -beta + 1, false, ply + 1,
                              thread_data, MOVE_NONE, false);
        board.undo_move(null_state);
        if (stop_.load(std::memory_order_relaxed)) return beta;
        if (score >= beta) {
            return score;
        }
    }

    if (!pv_node && !in_check) {
        int margin = tuning_.beta_margin(depth);
        if (static_eval - margin >= beta) {
            return static_eval - margin;
        }
    }

    if (!in_iid && depth >= 6 && !in_check && tt_move == MOVE_NONE &&
        std::abs(beta) < kMateThreshold) {
        int iid_depth = depth - 2;
        int iid_beta = std::min(beta, alpha + 1);
        int iid_score = negamax(board, iid_depth, alpha, iid_beta, pv_node, ply, thread_data,
                                 prev_move, true);
        if (stop_.load(std::memory_order_relaxed)) {
            return iid_score;
        }
        probe_tt(board, depth, alpha, beta, tt_move, tt_score, ply, tt_depth, tt_flag, tt_eval);
        static_eval = tt_eval;
    }

    auto moves = board.generate_legal_moves();
    if (moves.empty()) {
        if (board.side_to_move_in_check()) return -kMateValue + ply;
        return 0;
    }

    if (!pv_node && depth >= 5 && !in_check && std::abs(beta) < kMateThreshold) {
        int probcut_beta = beta + tuning_.probcut_margin();
        int probcut_alpha = probcut_beta - 1;
        int probcut_depth = depth - 3;
        for (Move move : moves) {
            if (!move_is_capture(move) && move_promo(move) == 0) continue;
            if (static_exchange_eval(board, move) < -80) continue;
            Board::State pc_state;
            board.apply_move(move, pc_state);
            int score = -negamax(board, probcut_depth, -probcut_beta, -probcut_alpha, false,
                                  ply + 1, thread_data, move, false);
            board.undo_move(pc_state);
            if (stop_.load(std::memory_order_relaxed)) return probcut_beta;
            if (score >= probcut_beta) return score;
        }
    }

    auto ordered = order_moves(board, moves, tt_move, ply, thread_data, prev_move);

    int best_score = -kInfiniteScore;
    Move best_move = MOVE_NONE;
    int alpha_orig = alpha;
    bool first = true;
    int move_count = 0;

    bool singular_ready = (!in_check && tt_move != MOVE_NONE && tt_flag == TT_LOWER &&
                           tt_depth >= depth - 1 && depth >= 4 &&
                           std::abs(tt_score) < kMateThreshold);
    int singular_beta = tt_score - tuning_.singular_margin_scale() * depth;
    int singular_reduction = tuning_.singular_reduction_base() +
                             depth / std::max(1, tuning_.null_move_depth_divisor());
    int singular_depth = depth - 1 - singular_reduction;
    bool allow_singular = singular_ready && singular_depth >= 1 && singular_beta > alpha;

    for (Move move : ordered) {
        ++move_count;
        int prev_alpha = alpha;
        bool is_capture = move_is_capture(move);
        bool is_promo = move_promo(move) != 0;
        bool quiet = !is_capture && !is_promo;
        int history_score = thread_data.history[static_cast<size_t>(history_index(move))];

        if (!pv_node && is_capture && depth <= 1 && static_exchange_eval(board, move) < 0) {
            continue;
        }

        int extension = 0;
        if (allow_singular && move == tt_move) {
            int best_other = singular_beta - 1;
            for (Move alt : moves) {
                if (alt == move) continue;
                Board::State singular_state;
                board.apply_move(alt, singular_state);
                int score = -negamax(board, singular_depth, -singular_beta, -best_other, false,
                                      ply + 1, thread_data, alt, false);
                board.undo_move(singular_state);
                if (stop_.load(std::memory_order_relaxed)) break;
                if (score >= singular_beta) {
                    best_other = score;
                    break;
                }
                best_other = std::max(best_other, score);
            }
            if (stop_.load(std::memory_order_relaxed)) return alpha;
            if (best_other < singular_beta) {
                extension = 1;
            }
        }

        Board::State state;
        board.apply_move(move, state);
        bool gives_check = board.side_to_move_in_check();

        if (!in_check && quiet && depth <= 3 && !gives_check) {
            int margin = tuning_.futility_margin(depth);
            if (static_eval + margin <= alpha) {
                board.undo_move(state);
                continue;
            }
        }

        if (!pv_node && quiet && !in_check && !gives_check) {
            int lmp_limit = tuning_.late_move_limit(depth, move_overhead_ms_, history_score, move_count);
            if (move_count > std::max(1, lmp_limit)) {
                board.undo_move(state);
                continue;
            }
        }

        if (!pv_node && quiet && depth <= 3 && move_count > 4 + depth && history_score < 0) {
            board.undo_move(state);
            continue;
        }

        int score;
        int child_depth = std::max(0, depth - 1 + extension);
        if (first) {
            score = -negamax(board, child_depth, -beta, -alpha, pv_node, ply + 1, thread_data,
                              move, false);
            first = false;
        } else {
            int new_depth = child_depth;
            bool apply_lmr = new_depth > 0 && depth >= 3 && move_count > 3 && quiet &&
                             !gives_check && !pv_node;
            if (apply_lmr) {
                double reduction_value = 1.0 + (depth > 4) + (move_count > 4) + (move_count > 8);
                if (history_score < 0) {
                    reduction_value += 1.0;
                }
                reduction_value *= tuning_.lmr_scale();
                reduction_value += (move_overhead_ms_ / 60.0) * tuning_.lmr_scale();
                int reduction = std::min(new_depth, std::max(1, static_cast<int>(std::round(reduction_value))));
                int reduced_depth = std::max(1, new_depth - reduction);
                score = -negamax(board, reduced_depth, -alpha - 1, -alpha, false, ply + 1,
                                  thread_data, move, false);
                if (score > alpha) {
                    score = -negamax(board, new_depth, -alpha - 1, -alpha, false, ply + 1,
                                      thread_data, move, false);
                }
            } else {
                score = -negamax(board, new_depth, -alpha - 1, -alpha, false, ply + 1,
                                  thread_data, move, false);
            }
            if (score > alpha && score < beta) {
                score = -negamax(board, new_depth, -beta, -alpha, true, ply + 1, thread_data,
                                  move, false);
            }
        }

        board.undo_move(state);

        if (stop_.load(std::memory_order_relaxed)) return score;

        if (score > best_score) {
            best_score = score;
            best_move = move;
        }
        if (score > alpha) {
            alpha = score;
        }
        bool improved = alpha > prev_alpha;
        if (alpha >= beta) {
            if (quiet) {
                update_killers(thread_data, ply, move);
                update_history(thread_data, move, depth * depth);
                if (prev_move != MOVE_NONE) {
                    thread_data.countermoves[static_cast<size_t>(history_index(prev_move))] = move;
                }
            }
            best_score = alpha;
            break;
        }
        if (quiet && !improved) {
            update_history(thread_data, move, -std::max(1, depth));
        }
    }

    int flag = TT_EXACT;
    if (best_score <= alpha_orig) flag = TT_UPPER;
    else if (best_score >= beta) flag = TT_LOWER;

    store_tt(board.zobrist_key(), best_move, depth, best_score, flag, ply, static_eval);
    return best_score;
}

int Search::quiescence(Board& board, int alpha, int beta, int ply, ThreadData& thread_data) {
    if (stop_.load(std::memory_order_relaxed)) return alpha;
    if (deadline_ && std::chrono::steady_clock::now() >= *deadline_) {
        stop_.store(true, std::memory_order_relaxed);
        return evaluate(board);
    }

    uint64_t visited = inc_node(thread_data.id);
    if (nodes_limit_ >= 0 && visited >= static_cast<uint64_t>(nodes_limit_)) {
        stop_.store(true, std::memory_order_relaxed);
        return evaluate(board);
    }
    int stand_pat = evaluate(board);
    if (stand_pat >= beta) return stand_pat;
    if (stand_pat > alpha) alpha = stand_pat;

    auto moves = board.generate_legal_moves();
    auto& capture_entries = thread_data.quiescence_captures;
    auto& quiet_checks = thread_data.quiescence_checks;
    capture_entries.clear();
    quiet_checks.clear();
    capture_entries.reserve(moves.size());
    quiet_checks.reserve(moves.size());

    for (Move move : moves) {
        bool is_capture = move_is_capture(move);
        bool is_promo = move_promo(move) != 0;
        if (is_capture || is_promo) {
            int see = static_exchange_eval(board, move);
            capture_entries.push_back({move, see});
        } else {
            quiet_checks.push_back(move);
        }
    }

    std::sort(capture_entries.begin(), capture_entries.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.see != rhs.see) return lhs.see > rhs.see;
        return lhs.move < rhs.move;
    });

    for (const auto& entry : capture_entries) {
        Move move = entry.move;
        int see = entry.see;
        Board::State state;
        bool applied = false;
        if (see < 0) {
            board.apply_move(move, state);
            applied = true;
            if (!board.side_to_move_in_check()) {
                board.undo_move(state);
                continue;
            }
        }
        if (!applied) {
            board.apply_move(move, state);
        }
        int score = -quiescence(board, -beta, -alpha, ply + 1, thread_data);
        board.undo_move(state);
        if (score >= beta) return score;
        if (score > alpha) alpha = score;
    }

    for (Move move : quiet_checks) {
        Board::State state;
        board.apply_move(move, state);
        if (!board.side_to_move_in_check()) {
            board.undo_move(state);
            continue;
        }
        int score = -quiescence(board, -beta, -alpha, ply + 1, thread_data);
        board.undo_move(state);
        if (score >= beta) return score;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

bool Search::probe_tt(const Board& board, int depth, int alpha, int beta, Move& tt_move,
                      int& score, int ply, int& tt_depth, int& tt_flag, int& tt_eval) const {
    if (tt_.empty()) {
        tt_move = MOVE_NONE;
        tt_depth = -1;
        tt_flag = TT_EXACT;
        return false;
    }

    Move move = MOVE_NONE;
    int stored_depth = -1;
    int stored_flag = TT_EXACT;
    int stored_eval = tt_eval;
    uint64_t key = board.zobrist_key();
    bool usable = tt_.probe(key, depth, alpha, beta, ply, move, score, stored_depth, stored_flag,
                            stored_eval);
    tt_move = move;
    tt_depth = stored_depth;
    tt_flag = stored_flag;
    tt_eval = stored_eval;
    return usable;
}

void Search::store_tt(uint64_t key, Move best, int depth, int score, int flag, int ply, int eval) {
    if (tt_.empty()) return;
    tt_.store(key, best, depth, score, flag, ply, eval);
}

std::vector<Move> Search::order_moves(const Board& board, std::vector<Move>& moves, Move tt_move,
                                      int ply, const ThreadData& thread_data, Move prev_move) const {
    std::vector<std::pair<int, Move>> scored;
    scored.reserve(moves.size());
    Move counter = MOVE_NONE;
    if (prev_move != MOVE_NONE) {
        counter = thread_data.countermoves[static_cast<size_t>(history_index(prev_move))];
    }
    for (Move move : moves) {
        int score = 0;
        if (move == tt_move) {
            score = 1'000'000;
        } else if (move_is_capture(move)) {
            char captured = board.piece_on(move_to(move));
            if (move_is_enpassant(move)) captured = board.white_to_move() ? 'p' : 'P';
            char mover = board.piece_on(move_from(move));
            score = 500'000 + piece_value(captured) * 10 - piece_value(mover);
        } else if (move_promo(move) != 0) {
            score = 400'000 + move_promo(move) * 100;
        } else {
            if (ply < static_cast<int>(thread_data.killers.size())) {
                const auto& killers = thread_data.killers[ply];
                if (move == killers[0]) score = 300'000;
                else if (move == killers[1]) score = 299'000;
            }
            if (score == 0 && counter != MOVE_NONE && move == counter) {
                score = 298'500;
            }
            if (score == 0) {
                score = 200'000 + thread_data.history[static_cast<size_t>(history_index(move))];
            }
        }
        scored.emplace_back(score, move);
    }
    std::sort(scored.begin(), scored.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first > rhs.first;
    });
    std::vector<Move> ordered;
    ordered.reserve(scored.size());
    for (auto& [score, move] : scored) ordered.push_back(move);
    return ordered;
}

void Search::update_killers(ThreadData& thread_data, int ply, Move move) {
    if (ply >= static_cast<int>(thread_data.killers.size())) {
        thread_data.killers.resize(ply + 1, {MOVE_NONE, MOVE_NONE});
    }
    auto& killers = thread_data.killers[ply];
    if (killers[0] != move) {
        killers[1] = killers[0];
        killers[0] = move;
    }
}

void Search::update_history(ThreadData& thread_data, Move move, int delta) {
    int idx = history_index(move);
    auto& entry = thread_data.history[static_cast<size_t>(idx)];
    entry = std::clamp(entry + delta, kHistoryMin, kHistoryMax);
}

std::vector<Move> Search::extract_pv(const Board& board, Move best) const {
    std::vector<Move> pv;
    if (best == MOVE_NONE) return pv;
    pv.push_back(best);
    Board current = board;
    std::vector<Board::State> states;
    states.reserve(kMaxPly);
    Board::State state;
    current.apply_move(best, state);
    states.push_back(state);
    for (int depth = 1; depth < kMaxPly; ++depth) {
        uint64_t key = current.zobrist_key();
        Move next = tt_.probe_move(key);
        if (next == MOVE_NONE) break;
        if (std::find(pv.begin(), pv.end(), next) != pv.end()) break;
        pv.push_back(next);
        Board::State next_state;
        current.apply_move(next, next_state);
        states.push_back(next_state);
    }
    for (auto it = states.rbegin(); it != states.rend(); ++it) {
        current.undo_move(*it);
    }
    return pv;
}

int Search::evaluate(const Board& board) const {
    if (use_nnue_eval_ && nnue_eval_) {
        return nnue_eval_->eval_cp(board);
    }
    return eval::evaluate(board);
}

std::optional<int> Search::probe_syzygy(const Board& board, int depth,
                                        bool root_probe) const {
    if (!syzygy_config_.enabled || syzygy_config_.path.empty()) {
        return std::nullopt;
    }
    if (!root_probe && depth < syzygy_config_.probe_depth) {
        return std::nullopt;
    }
    if (syzygy_config_.probe_limit >= 0 &&
        syzygy::TB::pieceCount(board) > syzygy_config_.probe_limit) {
        return std::nullopt;
    }

    auto probe = syzygy::TB::probePosition(board, syzygy_config_, root_probe);
    if (!probe) return std::nullopt;
    return tablebase_score(*probe, syzygy_config_);
}

} // namespace engine
