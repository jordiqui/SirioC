#include "engine/eval/eval.hpp"

#include <algorithm>
#include <array>
#include <cctype>

namespace engine::eval {
namespace {

constexpr int mirror_square(int sq) { return sq ^ 56; }

constexpr std::array<int, 6> kMgPieceValues{82, 337, 365, 477, 1025, 0};
constexpr std::array<int, 6> kEgPieceValues{94, 281, 297, 512, 936, 0};
constexpr std::array<int, 6> kGamePhaseInc{0, 1, 1, 2, 4, 0};

constexpr std::array<int, 64> kMgPawnTable{
     0,  0,  0,  0,  0,  0,  0,  0,
    12, 16, 20, 24, 24, 20, 16, 12,
     8, 12, 16, 20, 20, 16, 12,  8,
     4,  8, 12, 16, 16, 12,  8,  4,
     2,  4,  8, 12, 12,  8,  4,  2,
     0,  0,  0,  4,  4,  0,  0,  0,
     0,  0, -4, -8, -8, -4,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0};

constexpr std::array<int, 64> kMgKnightTable{
   -50, -40, -30, -30, -30, -30, -40, -50,
   -40, -20,   0,   0,   0,   0, -20, -40,
   -30,   0,  10,  15,  15,  10,   0, -30,
   -30,   5,  15,  20,  20,  15,   5, -30,
   -30,   0,  15,  20,  20,  15,   0, -30,
   -30,   5,  10,  15,  15,  10,   5, -30,
   -40, -20,   0,   5,   5,   0, -20, -40,
   -50, -40, -30, -30, -30, -30, -40, -50};

constexpr std::array<int, 64> kMgBishopTable{
   -20, -10, -10, -10, -10, -10, -10, -20,
   -10,   0,   0,   0,   0,   0,   0, -10,
   -10,   0,   5,  10,  10,   5,   0, -10,
   -10,   5,   5,  10,  10,   5,   5, -10,
   -10,   0,  10,  10,  10,  10,   0, -10,
   -10,  10,  10,  10,  10,  10,  10, -10,
   -10,   5,   0,   0,   0,   0,   5, -10,
   -20, -10, -10, -10, -10, -10, -10, -20};

constexpr std::array<int, 64> kMgRookTable{
     0,   0,   5,  10,  10,   5,   0,   0,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
     5,  10,  10,  10,  10,  10,  10,   5,
     0,   0,   0,   5,   5,   0,   0,   0};

constexpr std::array<int, 64> kMgQueenTable{
   -20, -10, -10,  -5,  -5, -10, -10, -20,
   -10,   0,   0,   0,   0,   0,   0, -10,
   -10,   0,   5,   5,   5,   5,   0, -10,
    -5,   0,   5,   5,   5,   5,   0,  -5,
     0,   0,   5,   5,   5,   5,   0,  -5,
   -10,   5,   5,   5,   5,   5,   0, -10,
   -10,   0,   5,   0,   0,   0,   0, -10,
   -20, -10, -10,  -5,  -5, -10, -10, -20};

constexpr std::array<int, 64> kMgKingTable{
   -30, -40, -40, -50, -50, -40, -40, -30,
   -30, -40, -40, -50, -50, -40, -40, -30,
   -30, -40, -40, -50, -50, -40, -40, -30,
   -30, -40, -40, -50, -50, -40, -40, -30,
   -20, -30, -30, -40, -40, -30, -30, -20,
   -10, -20, -20, -20, -20, -20, -20, -10,
    20,  20,   0,   0,   0,   0,  20,  20,
    20,  30,  10,   0,   0,  10,  30,  20};

constexpr std::array<int, 64> kEgPawnTable{
     0,  0,  0,  0,  0,  0,  0,  0,
    10, 10, 10, 10, 10, 10, 10, 10,
     5,  5, 10, 15, 15, 10,  5,  5,
     0,  0,  0, 10, 10,  0,  0,  0,
     5,  5,  5, 10, 10,  5,  5,  5,
    10, 10, 10, 20, 20, 10, 10, 10,
    50, 50, 50, 50, 50, 50, 50, 50,
     0,  0,  0,  0,  0,  0,  0,  0};

constexpr std::array<int, 64> kEgKnightTable{
   -40, -20, -10, -10, -10, -10, -20, -40,
   -20,  -5,   0,   0,   0,   0,  -5, -20,
   -10,   0,  10,  15,  15,  10,   0, -10,
   -10,   5,  15,  20,  20,  15,   5, -10,
   -10,   0,  15,  20,  20,  15,   0, -10,
   -10,   5,  10,  15,  15,  10,   5, -10,
   -20,  -5,   0,   5,   5,   0,  -5, -20,
   -40, -20, -10, -10, -10, -10, -20, -40};

constexpr std::array<int, 64> kEgBishopTable{
   -20, -10, -10, -10, -10, -10, -10, -20,
   -10,   0,   0,   0,   0,   0,   0, -10,
   -10,   0,  10,  15,  15,  10,   0, -10,
   -10,  10,  15,  20,  20,  15,  10, -10,
   -10,   0,  15,  20,  20,  15,   0, -10,
   -10,  10,  10,  15,  15,  10,  10, -10,
   -10,   5,   0,   5,   5,   0,   5, -10,
   -20, -10, -10, -10, -10, -10, -10, -20};

constexpr std::array<int, 64> kEgRookTable{
     0,   0,   0,   5,   5,   0,   0,   0,
     0,   0,   0,  10,  10,   0,   0,   0,
    -5,   0,   0,   5,   5,   0,   0,  -5,
    -5,   0,   0,   5,   5,   0,   0,  -5,
    -5,   0,   0,   5,   5,   0,   0,  -5,
    -5,   0,   0,   5,   5,   0,   0,  -5,
     5,  10,  10,  10,  10,  10,  10,   5,
     0,   0,   0,   5,   5,   0,   0,   0};

constexpr std::array<int, 64> kEgQueenTable{
   -20, -10, -10,  -5,  -5, -10, -10, -20,
   -10,   0,   0,   0,   0,   0,   0, -10,
   -10,   0,   5,   5,   5,   5,   0, -10,
    -5,   0,   5,   5,   5,   5,   0,  -5,
     0,   0,   5,   5,   5,   5,   0,  -5,
   -10,   5,   5,   5,   5,   5,   0, -10,
   -10,   0,   5,   0,   0,   0,   0, -10,
   -20, -10, -10,  -5,  -5, -10, -10, -20};

constexpr std::array<int, 64> kEgKingTable{
   -50, -40, -30, -20, -20, -30, -40, -50,
   -30, -20, -10,   0,   0, -10, -20, -30,
   -30, -10,   0,  10,  10,   0, -10, -30,
   -30, -10,  10,  20,  20,  10, -10, -30,
   -30, -10,  10,  20,  20,  10, -10, -30,
   -30, -10,   0,  10,  10,   0, -10, -30,
   -30, -30,   0,   0,   0,   0, -30, -30,
   -50, -30, -30, -30, -30, -30, -30, -50};

constexpr std::array<std::array<int, 64>, 6> kMgPst{
    kMgPawnTable,
    kMgKnightTable,
    kMgBishopTable,
    kMgRookTable,
    kMgQueenTable,
    kMgKingTable,
};

constexpr std::array<std::array<int, 64>, 6> kEgPst{
    kEgPawnTable,
    kEgKnightTable,
    kEgBishopTable,
    kEgRookTable,
    kEgQueenTable,
    kEgKingTable,
};

constexpr int kGamePhaseMax = 24;

int piece_index(char piece) {
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

} // namespace

int evaluate(const Board& board) {
    int mg_score = 0;
    int eg_score = 0;
    int phase = 0;

    for (int sq = 0; sq < 64; ++sq) {
        char piece = board.squares()[sq];
        if (piece == '.') continue;
        int idx = piece_index(piece);
        if (idx < 0) continue;
        bool white = std::isupper(static_cast<unsigned char>(piece));
        int table_sq = white ? sq : mirror_square(sq);
        int mg = kMgPieceValues[idx] + kMgPst[idx][table_sq];
        int eg = kEgPieceValues[idx] + kEgPst[idx][table_sq];
        if (white) {
            mg_score += mg;
            eg_score += eg;
        } else {
            mg_score -= mg;
            eg_score -= eg;
        }
        phase += kGamePhaseInc[idx];
    }

    phase = std::clamp(phase, 0, kGamePhaseMax);
    int eg_phase = kGamePhaseMax - phase;
    int score = (mg_score * phase + eg_score * eg_phase) / kGamePhaseMax;

    // Tempo bonus encourages the side to move slightly.
    score += board.white_to_move() ? 10 : -10;

    return board.white_to_move() ? score : -score;
}

} // namespace engine::eval
