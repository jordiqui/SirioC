#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace engine {

using Move = uint32_t; // Encode from/to squares, promotion, flags
constexpr Move MOVE_NONE = 0;

constexpr int MOVE_FROM_SHIFT = 0;
constexpr int MOVE_TO_SHIFT = 6;
constexpr int MOVE_PROMO_SHIFT = 12;
constexpr Move MOVE_PROMO_MASK = 0b111u << MOVE_PROMO_SHIFT;

constexpr Move MOVE_FLAG_CAPTURE = 1u << 15;
constexpr Move MOVE_FLAG_DOUBLE_PAWN = 1u << 16;
constexpr Move MOVE_FLAG_ENPASSANT = 1u << 17;
constexpr Move MOVE_FLAG_CASTLING = 1u << 18;

constexpr Move make_move(int from, int to, int promo = 0, bool capture = false,
                         bool double_pawn = false, bool enpassant = false,
                         bool castling = false) {
    return (static_cast<Move>(from & 63) << MOVE_FROM_SHIFT) |
           (static_cast<Move>(to & 63) << MOVE_TO_SHIFT) |
           (static_cast<Move>(promo & 7) << MOVE_PROMO_SHIFT) |
           (capture ? MOVE_FLAG_CAPTURE : 0) |
           (double_pawn ? MOVE_FLAG_DOUBLE_PAWN : 0) |
           (enpassant ? MOVE_FLAG_ENPASSANT : 0) |
           (castling ? MOVE_FLAG_CASTLING : 0);
}

constexpr int move_from(Move m) { return (m >> MOVE_FROM_SHIFT) & 63; }
constexpr int move_to(Move m) { return (m >> MOVE_TO_SHIFT) & 63; }
constexpr int move_promo(Move m) { return (m >> MOVE_PROMO_SHIFT) & 7; }
constexpr bool move_is_capture(Move m) { return (m & MOVE_FLAG_CAPTURE) != 0; }
constexpr bool move_is_double_pawn(Move m) {
    return (m & MOVE_FLAG_DOUBLE_PAWN) != 0;
}
constexpr bool move_is_enpassant(Move m) {
    return (m & MOVE_FLAG_ENPASSANT) != 0;
}
constexpr bool move_is_castling(Move m) {
    return (m & MOVE_FLAG_CASTLING) != 0;
}

struct Limits {
    int32_t depth = 64;
    int64_t movetime_ms = -1;
    int64_t wtime_ms = -1, btime_ms = -1, winc_ms = 0, binc_ms = 0;
    int64_t nodes = -1;
};

struct ParsedGo {
    Limits limits;
};

} // namespace engine
