"""Evaluate a trained Sirio NNUE checkpoint."""
from __future__ import annotations

import argparse
import json
import pathlib
from typing import Any, Dict

import torch

from .dataset import PieceCountDataset
from .train import PieceCountModel, compute_metrics, set_random_seeds


def load_checkpoint(path: str | pathlib.Path) -> Dict[str, Any]:
    checkpoint = torch.load(path, map_location="cpu")
    if "model_state" not in checkpoint:
        raise ValueError("Checkpoint missing 'model_state'")
    return checkpoint


def evaluate_checkpoint(checkpoint_path: str, dataset_path: str, batch_size: int, seed: int) -> Dict[str, float]:
    set_random_seeds(seed)
    dataset = PieceCountDataset(dataset_path)
    loader = torch.utils.data.DataLoader(dataset, batch_size=batch_size, shuffle=False)

    model = PieceCountModel()
    checkpoint = load_checkpoint(checkpoint_path)
    model.load_state_dict(checkpoint["model_state"])
    model.eval()

    preds: list[torch.Tensor] = []
    tgts: list[torch.Tensor] = []
    plies: list[torch.Tensor] = []
    with torch.no_grad():
        for features, targets, ply in loader:
            preds.append(model(features))
            tgts.append(targets)
            plies.append(ply)

    predictions = torch.cat(preds)
    targets = torch.cat(tgts)
    ply_tensor = torch.cat(plies) if plies else None
    return compute_metrics(predictions, targets, ply_tensor)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checkpoint", required=True, help="Path to the .pt checkpoint")
    parser.add_argument("--dataset", required=True, help="Path to the dataset .npz")
    parser.add_argument("--batch-size", type=int, default=2048)
    parser.add_argument("--seed", type=int, default=20240511)
    parser.add_argument("--output", help="Optional path to write metrics as JSON")
    args = parser.parse_args()

    metrics = evaluate_checkpoint(args.checkpoint, args.dataset, args.batch_size, args.seed)
    if args.output:
        path = pathlib.Path(args.output)
        path.parent.mkdir(parents=True, exist_ok=True)
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(metrics, handle, indent=2)
    else:
        print(json.dumps(metrics, indent=2))


if __name__ == "__main__":
    main()

