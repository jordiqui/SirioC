"""Feature extraction helpers for the Sirio NNUE training pipeline."""
from __future__ import annotations

import dataclasses
from typing import Iterable, List

import chess

PIECE_ORDER: List[chess.PieceType] = [
    chess.PAWN,
    chess.KNIGHT,
    chess.BISHOP,
    chess.ROOK,
    chess.QUEEN,
    chess.KING,
]

MAX_PIECE_COUNT = 8  # Normalisation factor used for every slot.
PIECE_VALUES = {
    chess.PAWN: 100,
    chess.KNIGHT: 320,
    chess.BISHOP: 330,
    chess.ROOK: 500,
    chess.QUEEN: 900,
    chess.KING: 0,
}


@dataclasses.dataclass(frozen=True)
class FeatureVector:
    """Container that stores normalised NNUE features for a board position."""

    counts: List[float]

    def as_list(self) -> List[float]:
        return list(self.counts)


def _count_pieces(board: chess.Board, color: chess.Color, piece: chess.PieceType) -> int:
    return len(board.pieces(piece, color))


def encode_piece_counts(board: chess.Board) -> FeatureVector:
    """Encode the board as Sirio piece-count features.

    Returns:
        FeatureVector: 12 floating-point values in the order expected by the C++
        backend (white pieces followed by black pieces).
    """

    counts: List[float] = []
    for color in (chess.WHITE, chess.BLACK):
        for piece in PIECE_ORDER:
            count = _count_pieces(board, color, piece)
            counts.append(count / MAX_PIECE_COUNT)
    return FeatureVector(counts)


def material_balance(board: chess.Board) -> int:
    """Return the material balance in centipawns from White's perspective."""

    material = 0
    for color in (chess.WHITE, chess.BLACK):
        sign = 1 if color == chess.WHITE else -1
        for piece_type in PIECE_ORDER:
            piece_count = _count_pieces(board, color, piece_type)
            material += sign * piece_count * PIECE_VALUES[piece_type]
    return material


def result_to_score(result: str | None) -> float:
    """Map PGN results to scores in [-1, 1]."""

    if not result:
        return 0.0
    result = result.strip()
    if result == "1-0":
        return 1.0
    if result == "0-1":
        return -1.0
    if result in {"1/2-1/2", "1/2", "0.5-0.5"}:
        return 0.0
    return 0.0


def encode_target(board: chess.Board, result: str | None, result_weight: float) -> float:
    """Compute the supervised target for the given position.

    The target combines a simple material evaluation with the game outcome so the
    network learns to prefer positions that eventually lead to victory.
    """

    material = material_balance(board)
    side_to_move = board.turn
    if side_to_move == chess.BLACK:
        material = -material

    outcome = result_to_score(result)
    if side_to_move == chess.BLACK:
        outcome = -outcome
    return material + result_weight * outcome


def ply_count(board: chess.Board) -> int:
    """Return the half-move count of the current position."""

    return board.fullmove_number * 2 - (0 if board.turn == chess.WHITE else 1)


def feature_vector_from_board(board: chess.Board) -> Iterable[float]:
    """Convenience helper that yields the 12 normalised features."""

    return encode_piece_counts(board).counts

