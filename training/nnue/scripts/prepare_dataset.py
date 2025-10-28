"""Prepare NNUE training datasets from PGN files."""
from __future__ import annotations

import argparse
import json
import pathlib
import random
import time
from typing import Iterable, Iterator, List, Sequence

import chess
import chess.pgn
import numpy as np
from tqdm import tqdm

from . import features


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pgn", nargs="+", required=True, help="Input PGN files")
    parser.add_argument("--output-dir", required=True, help="Directory where the dataset will be stored")
    parser.add_argument("--name", required=True, help="Dataset name (used as filename prefix)")
    parser.add_argument("--max-games", type=int, default=None, help="Optional limit on the number of games to parse")
    parser.add_argument("--sample-stride", type=int, default=2, help="Sample every N plies")
    parser.add_argument("--result-weight", type=float, default=400.0, help="Weight applied to the game outcome when computing targets")
    parser.add_argument("--min-ply", type=int, default=6, help="Ignore positions before this ply (opening book noise)")
    parser.add_argument("--limit-positions", type=int, default=None, help="Optional cap on the number of positions")
    parser.add_argument("--seed", type=int, default=20240511, help="Random seed")
    return parser.parse_args()


def _iter_games(paths: Sequence[str]) -> Iterator[chess.pgn.Game]:
    for path in paths:
        with open(path, "r", encoding="utf-8") as handle:
            while True:
                game = chess.pgn.read_game(handle)
                if game is None:
                    break
                yield game


def _sample_game(game: chess.pgn.Game, stride: int, min_ply: int) -> Iterable[chess.Board]:
    board = game.board()
    for ply, move in enumerate(game.mainline_moves(), start=1):
        board.push(move)
        if ply < min_ply:
            continue
        if ply % stride == 0:
            yield board.copy()


def build_dataset(args: argparse.Namespace) -> None:
    random.seed(args.seed)
    np.random.seed(args.seed)

    output_dir = pathlib.Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    features_list: List[List[float]] = []
    targets: List[float] = []
    ply_list: List[int] = []

    pgn_paths = list(args.pgn)
    result_weight = args.result_weight

    game_iterator = _iter_games(pgn_paths)
    progress = tqdm(total=args.limit_positions, desc="Extracting positions", unit="pos")
    games_processed = 0

    for game in game_iterator:
        if args.max_games is not None and games_processed >= args.max_games:
            break
        result = game.headers.get("Result")
        for board in _sample_game(game, args.sample_stride, args.min_ply):
            features_vector = list(features.feature_vector_from_board(board))
            target = features.encode_target(board, result, result_weight)
            features_list.append(features_vector)
            targets.append(target)
            ply_list.append(features.ply_count(board))
            progress.update(1)

            if args.limit_positions is not None and len(features_list) >= args.limit_positions:
                break
        games_processed += 1
        if args.limit_positions is not None and len(features_list) >= args.limit_positions:
            break
    progress.close()

    if not features_list:
        raise RuntimeError("No positions extracted; check PGN input")

    features_array = np.asarray(features_list, dtype=np.float32)
    targets_array = np.asarray(targets, dtype=np.float32)
    ply_array = np.asarray(ply_list, dtype=np.uint16)

    dataset_path = output_dir / f"{args.name}.npz"
    np.savez_compressed(dataset_path, features=features_array, targets=targets_array, ply=ply_array)

    metadata = {
        "name": args.name,
        "version": 1,
        "seed": args.seed,
        "source": pgn_paths,
        "samples": int(features_array.shape[0]),
        "sample_stride": args.sample_stride,
        "result_weight": result_weight,
        "target": "material+result",
        "created_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    }
    metadata_path = output_dir / f"{args.name}.metadata.json"
    with open(metadata_path, "w", encoding="utf-8") as handle:
        json.dump(metadata, handle, indent=2)

    print(f"Dataset written to {dataset_path} with {features_array.shape[0]} samples")


def main() -> None:
    args = parse_args()
    build_dataset(args)


if __name__ == "__main__":
    main()

