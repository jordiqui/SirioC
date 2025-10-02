#include "zobrist.h"

#include <array>
#include <atomic>
#include <cstddef>

namespace {

constexpr uint64_t kPieceSeed = 0x8244F4A2C5D39D11ULL;
constexpr uint64_t kSideSeed = 0x56A9D3C94B7E1F23ULL;

std::atomic<bool> g_initialized{false};
std::array<std::array<std::array<uint64_t, 64>, PIECE_TYPE_NB>, COLOR_NB> g_piece_keys{};
uint64_t g_side_key = 0ULL;

uint64_t splitmix64(uint64_t& state) {
    state += 0x9E3779B97F4A7C15ULL;
    uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z ^= z >> 31;
    return z;
}

void initialize_keys() {
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) {
        return;
    }

    uint64_t state = kPieceSeed;
    for (int color = 0; color < COLOR_NB; ++color) {
        for (int piece = 0; piece < PIECE_TYPE_NB; ++piece) {
            for (int square = 0; square < 64; ++square) {
                g_piece_keys[color][piece][square] = splitmix64(state);
            }
        }
    }

    state ^= kSideSeed;
    g_side_key = splitmix64(state);
}

}  // namespace

extern "C" {

void zobrist_init(void) {
    initialize_keys();
}

uint64_t zobrist_piece_key(enum Color color, Piece piece, Square square) {
    initialize_keys();
    if (color < 0 || color >= COLOR_NB || piece <= PIECE_NONE || piece >= PIECE_TYPE_NB || square < 0 || square >= 64) {
        return 0ULL;
    }
    return g_piece_keys[color][piece][square];
}

uint64_t zobrist_side_key(void) {
    initialize_keys();
    return g_side_key;
}

uint64_t zobrist_compute_key(const Board* board) {
    initialize_keys();
    if (!board) {
        return 0ULL;
    }

    uint64_t key = 0ULL;
    for (Square square = 0; square < 64; ++square) {
        Piece piece = board->squares[square];
        if (piece == PIECE_NONE) {
            continue;
        }
        uint64_t bit = 1ULL << square;
        size_t white_index = (size_t)piece + (size_t)PIECE_TYPE_NB * (size_t)COLOR_WHITE;
        size_t black_index = (size_t)piece + (size_t)PIECE_TYPE_NB * (size_t)COLOR_BLACK;
        enum Color color = (board->bitboards[white_index] & bit) ? COLOR_WHITE : COLOR_BLACK;
        if (!(board->bitboards[white_index] & bit) && !(board->bitboards[black_index] & bit)) {
            continue;
        }
        key ^= g_piece_keys[color][piece][square];
    }

    if (board->side_to_move == COLOR_BLACK) {
        key ^= g_side_key;
    }

    return key;
}

}  // extern "C"

