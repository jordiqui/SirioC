# LEGACY NOTICE (P0-39): SirioNNUE1 / PieceCountModel pipeline.
# This module is retained for compatibility and test-baseline continuity only.
# New NNUE work should use v2 scripts: features_v2.py, prepare_dataset_v2.py,
# train_v2.py, export_to_engine_v2.py, build_candidate_v2.py,
# verify_candidate_v2.py, and verified_runtime_load_v2.py.
# Stockfish .nnue compatibility is not claimed.

"""Export trained weights to the `SirioNNUE1` text format."""
from __future__ import annotations

import argparse
import pathlib
from typing import Any, Dict

import torch

from .train import PieceCountModel


HEADER = "SirioNNUE1"


def load_checkpoint(path: str | pathlib.Path) -> Dict[str, Any]:
    checkpoint = torch.load(path, map_location="cpu")
    if "model_state" not in checkpoint:
        raise ValueError("Checkpoint missing 'model_state'")
    return checkpoint


def export(checkpoint_path: str, output_path: str) -> None:
    checkpoint = load_checkpoint(checkpoint_path)
    model = PieceCountModel()
    model.load_state_dict(checkpoint["model_state"])

    output = pathlib.Path(output_path)
    output.parent.mkdir(parents=True, exist_ok=True)

    with open(output, "w", encoding="utf-8") as handle:
        handle.write(f"{HEADER}\n")
        handle.write(f"{model.bias.item():.10f}\n")
        handle.write(f"{model.scale.item():.10f}\n")
        weights = model.weights.detach().cpu().numpy()
        handle.write(" ".join(f"{value:.10f}" for value in weights))
        handle.write("\n")

    print(f"Exported weights to {output}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checkpoint", required=True, help="Path to the trained .pt checkpoint")
    parser.add_argument("--output", required=True, help="Where to write the SirioNNUE1 file")
    args = parser.parse_args()
    export(args.checkpoint, args.output)


if __name__ == "__main__":
    main()

