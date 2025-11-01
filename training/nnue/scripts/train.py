"""Train the Sirio NNUE model on a prepared dataset."""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import random
from dataclasses import dataclass
from typing import Any, Dict, Tuple

import numpy as np
import torch
import yaml
from torch import nn
from torch.utils.data import DataLoader
from tqdm import tqdm

from .dataset import DatasetMetadata, PieceCountDataset, deterministic_split


@dataclass
class TrainingConfig:
    epochs: int
    batch_size: int
    learning_rate: float
    weight_decay: float
    device: str
    gradient_clip: float | None


class PieceCountModel(nn.Module):
    """Simple linear model matching SingleNetworkBackend."""

    def __init__(self) -> None:
        super().__init__()
        self.bias = nn.Parameter(torch.zeros(1))
        self.scale = nn.Parameter(torch.ones(1))
        self.weights = nn.Parameter(torch.zeros(12))

    def forward(self, x: torch.Tensor) -> torch.Tensor:  # type: ignore[override]
        logits = torch.matmul(x, self.weights) + self.bias
        return logits * self.scale


@dataclass
class RunState:
    model: PieceCountModel
    optimizer: torch.optim.Optimizer
    train_loader: DataLoader
    val_loader: DataLoader
    device: torch.device


def set_random_seeds(seed: int) -> None:
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)
    torch.cuda.manual_seed_all(seed)


def load_config(path: str | os.PathLike[str]) -> Tuple[dict[str, Any], TrainingConfig]:
    with open(path, "r", encoding="utf-8") as handle:
        raw = yaml.safe_load(handle)

    training_cfg = TrainingConfig(
        epochs=int(raw["training"]["epochs"]),
        batch_size=int(raw["training"]["batch_size"]),
        learning_rate=float(raw["training"]["learning_rate"]),
        weight_decay=float(raw["training"].get("weight_decay", 0.0)),
        device=str(raw["training"].get("device", "cpu")),
        gradient_clip=(
            float(raw["training"]["gradient_clip"])
            if raw["training"].get("gradient_clip") is not None
            else None
        ),
    )
    return raw, training_cfg


def make_dataloaders(config: dict[str, Any]) -> Tuple[PieceCountDataset, DataLoader, DataLoader, DatasetMetadata]:
    dataset_cfg = config["dataset"]
    dataset = PieceCountDataset(dataset_cfg["path"])
    metadata = DatasetMetadata.from_file(dataset_cfg["metadata"])

    train_subset, val_subset = deterministic_split(
        dataset, validation_split=float(dataset_cfg["validation_split"]), seed=int(dataset_cfg["shuffle_seed"])
    )

    shuffle = bool(dataset_cfg.get("shuffle", True))
    train_loader = DataLoader(train_subset, batch_size=int(config["training"]["batch_size"]), shuffle=shuffle)
    val_loader = DataLoader(val_subset, batch_size=int(config["training"]["batch_size"]), shuffle=False)
    return dataset, train_loader, val_loader, metadata


def compute_metrics(
    predictions: torch.Tensor, targets: torch.Tensor, ply: torch.Tensor | None = None
) -> Dict[str, float]:
    preds = predictions.detach().cpu()
    tgts = targets.detach().cpu()

    diff = preds - tgts
    mse = torch.mean(diff ** 2).item()
    mae = torch.mean(torch.abs(diff)).item()

    metrics: Dict[str, float] = {"mse": float(mse), "mae": float(mae), "bias": float(diff.mean().item())}

    if preds.numel() > 1:
        centered_pred = preds - preds.mean()
        centered_tgt = tgts - tgts.mean()
        denom = torch.linalg.vector_norm(centered_pred) * torch.linalg.vector_norm(centered_tgt)
        if denom.item() > 1e-12:
            pearson = torch.dot(centered_pred.view(-1), centered_tgt.view(-1)) / denom
            metrics["pearson_r"] = float(pearson.item())
        else:
            metrics["pearson_r"] = 0.0

    if ply is not None:
        ply_cpu = ply.detach().cpu()
        abs_diff = torch.abs(diff)
        phase_windows = {
            "early_game": (0, 20),
            "mid_game": (20, 50),
            "late_game": (50, 200),
        }
        for label, (lo, hi) in phase_windows.items():
            mask = (ply_cpu >= lo) & (ply_cpu < hi)
            if torch.count_nonzero(mask) > 0:
                metrics[f"{label}_mae"] = float(torch.mean(abs_diff[mask]).item())

    return metrics


def evaluate(model: PieceCountModel, loader: DataLoader, device: torch.device) -> Dict[str, float]:
    model.eval()
    preds: list[torch.Tensor] = []
    tgts: list[torch.Tensor] = []
    plies: list[torch.Tensor] = []
    with torch.no_grad():
        for features, targets, ply in loader:
            features = features.to(device)
            targets = targets.to(device)
            preds.append(model(features))
            tgts.append(targets)
            plies.append(ply)
    predictions = torch.cat(preds) if preds else torch.empty(0)
    targets = torch.cat(tgts) if tgts else torch.empty(0)
    ply_tensor = torch.cat(plies) if plies else None
    return compute_metrics(predictions, targets, ply_tensor)


def train_one_epoch(model: PieceCountModel, loader: DataLoader, optimizer: torch.optim.Optimizer, device: torch.device, gradient_clip: float | None) -> Dict[str, float]:
    model.train()
    total_loss = 0.0
    total_examples = 0
    total_abs_error = 0.0
    total_bias = 0.0
    for features, targets, _ in loader:
        features = features.to(device)
        targets = targets.to(device)
        optimizer.zero_grad()
        predictions = model(features)
        loss = torch.mean((predictions - targets) ** 2)
        loss.backward()
        if gradient_clip is not None:
            torch.nn.utils.clip_grad_norm_(model.parameters(), gradient_clip)
        optimizer.step()

        batch_size = features.shape[0]
        total_loss += loss.item() * batch_size
        total_abs_error += torch.sum(torch.abs(predictions - targets)).item()
        total_bias += torch.sum((predictions - targets).detach()).item()
        total_examples += batch_size
    mse = total_loss / max(1, total_examples)
    mae = total_abs_error / max(1, total_examples)
    bias = total_bias / max(1, total_examples)
    return {"mse": mse, "mae": mae, "bias": bias}


def save_checkpoint(model: PieceCountModel, path: pathlib.Path, metadata: Dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    state = {
        "model_state": model.state_dict(),
        "metadata": metadata,
    }
    torch.save(state, path)


def log_metrics(output_dir: pathlib.Path, run_name: str, epoch: int, train_metrics: Dict[str, float], val_metrics: Dict[str, float]) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    log_path = output_dir / f"{run_name}.jsonl"
    record = {
        "epoch": epoch,
        "train": train_metrics,
        "validation": val_metrics,
    }
    with open(log_path, "a", encoding="utf-8") as handle:
        handle.write(json.dumps(record) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True, help="Path to the YAML configuration")
    args = parser.parse_args()

    raw_config, training_cfg = load_config(args.config)
    set_random_seeds(int(raw_config.get("seed", 0)))

    dataset, train_loader, val_loader, dataset_metadata = make_dataloaders(raw_config)

    device = torch.device(training_cfg.device)
    model = PieceCountModel().to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=training_cfg.learning_rate, weight_decay=training_cfg.weight_decay)

    run_metadata = {
        "config": args.config,
        "dataset": raw_config["dataset"]["path"],
        "dataset_metadata": dataset_metadata.__dict__,
        "seed": raw_config.get("seed", 0),
    }

    best_val_mse = float("inf")
    checkpoint_cfg = raw_config["checkpoint"]
    checkpoint_path = pathlib.Path(checkpoint_cfg["output_path"])
    save_every = int(checkpoint_cfg.get("save_every", 1))

    progress = tqdm(range(1, training_cfg.epochs + 1), desc="Training", unit="epoch")
    for epoch in progress:
        train_metrics = train_one_epoch(model, train_loader, optimizer, device, training_cfg.gradient_clip)
        val_metrics = evaluate(model, val_loader, device)

        progress.set_postfix({"train_mse": train_metrics["mse"], "val_mse": val_metrics["mse"]})

        if epoch % save_every == 0 or val_metrics["mse"] < best_val_mse:
            metadata = {**run_metadata, "epoch": epoch, "val_metrics": val_metrics}
            save_checkpoint(model, checkpoint_path, metadata)

        if val_metrics["mse"] < best_val_mse:
            best_val_mse = val_metrics["mse"]

        log_cfg = raw_config["logging"]
        log_metrics(pathlib.Path(log_cfg["output_dir"]), str(log_cfg["run_name"]), epoch, train_metrics, val_metrics)

    progress.close()


if __name__ == "__main__":
    main()

