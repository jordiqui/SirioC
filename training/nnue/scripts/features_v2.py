"""SirioHalfKAv1 sparse feature encoder for parity verification."""
from __future__ import annotations

from dataclasses import dataclass

SIRIO_HALFKA_V1_ID = "SirioHalfKAv1"
PERSPECTIVE_COUNT = 2
RELATIVE_CHANNEL_COUNT = 10
SQUARE_COUNT = 64
FEATURES_PER_PERSPECTIVE = 40960

WHITE = "w"
BLACK = "b"

PIECE_TO_CHANNEL = {
    (WHITE, "P"): 0,
    (WHITE, "N"): 1,
    (WHITE, "B"): 2,
    (WHITE, "R"): 3,
    (WHITE, "Q"): 4,
    (BLACK, "P"): 5,
    (BLACK, "N"): 6,
    (BLACK, "B"): 7,
    (BLACK, "R"): 8,
    (BLACK, "Q"): 9,
}


@dataclass(frozen=True)
class SparseFeature:
    index: int
    value: int


def _parse_piece_placement(piece_placement: str) -> list[tuple[str, str, int]]:
    rows = piece_placement.split("/")
    if len(rows) != 8:
        raise ValueError(f"invalid FEN piece placement rows: expected 8, got {len(rows)}")

    pieces: list[tuple[str, str, int]] = []
    white_kings = 0
    black_kings = 0
    for rank_from_top, row in enumerate(rows):
        file_idx = 0
        for ch in row:
            if ch.isdigit():
                empty = int(ch)
                if empty < 1 or empty > 8:
                    raise ValueError(f"invalid FEN digit '{ch}' in rank '{row}'")
                file_idx += empty
                continue
            if ch not in {"P", "N", "B", "R", "Q", "K", "p", "n", "b", "r", "q", "k"}:
                raise ValueError(f"invalid FEN piece '{ch}'")
            if file_idx >= 8:
                raise ValueError(f"too many files in rank '{row}'")
            color = WHITE if ch.isupper() else BLACK
            piece = ch.upper()
            rank = 7 - rank_from_top
            square = rank * 8 + file_idx
            pieces.append((color, piece, square))
            if piece == "K":
                if color == WHITE:
                    white_kings += 1
                else:
                    black_kings += 1
            file_idx += 1
        if file_idx != 8:
            raise ValueError(f"invalid FEN rank width in '{row}': {file_idx}")

    if white_kings != 1 or black_kings != 1:
        raise ValueError(f"invalid king count: white={white_kings}, black={black_kings}")
    return pieces


def _parse_fen(fen: str) -> tuple[int, int, list[tuple[str, str, int]]]:
    fields = fen.strip().split()
    if len(fields) < 4:
        raise ValueError("invalid FEN: expected at least 4 fields")
    pieces = _parse_piece_placement(fields[0])
    white_king = next(square for color, piece, square in pieces if color == WHITE and piece == "K")
    black_king = next(square for color, piece, square in pieces if color == BLACK and piece == "K")
    return white_king, black_king, pieces


def perspective_square(perspective: str, square: int) -> int:
    if square < 0 or square >= SQUARE_COUNT:
        raise ValueError(f"invalid square {square}")
    if perspective == WHITE:
        return square
    file_idx = square % 8
    rank = square // 8
    return (7 - rank) * 8 + file_idx


def make_feature_index(perspective_king_square: int, relative_piece_channel: int, perspective_piece_square: int) -> int:
    return ((perspective_king_square * RELATIVE_CHANNEL_COUNT + relative_piece_channel) * SQUARE_COUNT + perspective_piece_square)


def encode_sirio_halfka_v1(fen: str) -> dict[str, list[SparseFeature]]:
    white_king, black_king, pieces = _parse_fen(fen)
    output: dict[str, list[SparseFeature]] = {WHITE: [], BLACK: []}

    for perspective in (WHITE, BLACK):
        king_square = white_king if perspective == WHITE else black_king
        perspective_king_square = perspective_square(perspective, king_square)
        active: list[SparseFeature] = []
        for piece_color in (WHITE, BLACK):
            for piece_type in ("P", "N", "B", "R", "Q"):
                channel = PIECE_TO_CHANNEL[(WHITE if piece_color == perspective else BLACK, piece_type)]
                for color, ptype, square in pieces:
                    if color == piece_color and ptype == piece_type:
                        piece_sq = perspective_square(perspective, square)
                        active.append(SparseFeature(index=make_feature_index(perspective_king_square, channel, piece_sq), value=1))
        output[perspective] = active
    return output
