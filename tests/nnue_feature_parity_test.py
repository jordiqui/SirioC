from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from training.nnue.scripts.features_v2 import BLACK, FEATURES_PER_PERSPECTIVE, WHITE, encode_sirio_halfka_v1


FENS = [
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "8/8/8/8/8/8/4k3/4K3 w - - 0 1",
    "8/8/8/3k4/8/8/4P3/4K3 w - - 0 1",
    "4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1",
]


def parse_dump(text: str) -> dict[str, dict[int, list[tuple[int, int]]]]:
    parsed: dict[str, dict[int, list[tuple[int, int]]]] = {}
    current_fen = ""
    for line in text.splitlines():
        if line.startswith("FEN="):
            current_fen = line[4:]
            parsed[current_fen] = {}
            continue
        if line.startswith("P") and current_fen:
            parts = line.split()
            perspective = int(parts[0][1:])
            features = []
            for token in parts[2:]:
                idx_str, val_str = token.split(":")
                features.append((int(idx_str), int(val_str)))
            parsed[current_fen][perspective] = features
    return parsed


def main() -> int:
    dump_bin = ROOT / "build" / "sirio_feature_dump"
    result = subprocess.run([str(dump_bin), *FENS], capture_output=True, text=True, check=True)
    cpp = parse_dump(result.stdout)

    for fen in FENS:
        py_state = encode_sirio_halfka_v1(fen)
        py_white = [(f.index, f.value) for f in py_state[WHITE]]
        py_black = [(f.index, f.value) for f in py_state[BLACK]]
        assert py_white == cpp[fen][0], f"white mismatch for {fen}"
        assert py_black == cpp[fen][1], f"black mismatch for {fen}"

    start = encode_sirio_halfka_v1(FENS[0])
    assert len(start[WHITE]) == 30
    assert len(start[BLACK]) == 30

    kings = encode_sirio_halfka_v1(FENS[1])
    assert len(kings[WHITE]) == 0
    assert len(kings[BLACK]) == 0

    mixed = encode_sirio_halfka_v1(FENS[3])
    for perspective in (WHITE, BLACK):
        indices = [f.index for f in mixed[perspective]]
        assert len(indices) == len(set(indices))
        assert all(0 <= i < FEATURES_PER_PERSPECTIVE for i in indices)

    print("Parity checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
