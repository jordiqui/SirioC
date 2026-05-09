"""Minimal deterministic SirioNNUE2 trainer scaffold for dataset-v2 JSONL files."""
from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import random
from dataclasses import dataclass
from typing import Any

import torch
import yaml
from torch import nn
from torch.utils.data import DataLoader, Dataset

from .features_v2 import SIRIO_HALFKA_V1_ID
from .nnue2_layout_contract import (
    ACCUMULATOR_SIZE,
    ACTIVATION,
    EXPECTED_SCRIPT_NAME as SCRIPT_NAME,
    FEATURE_SET,
    FEATURES_PER_PERSPECTIVE,
    HIDDEN1_SIZE,
    HIDDEN2_SIZE,
    MODEL_LAYOUT_NAME,
    MODEL_LAYOUT_VERSION,
    OUTPUT_SIZE,
    QUANT_INPUT_SCALE,
    QUANT_OUTPUT_SCALE,
    TENSOR_ORDER,
)

SCRIPT_VERSION = "p0-10-layout-v1"


@dataclass(frozen=True)
class Record:
    white_indices: list[int]
    black_indices: list[int]
    score_cp: float


class SparseJsonlDataset(Dataset[Record]):
    def __init__(self, path: pathlib.Path, max_records: int | None = None) -> None:
        self.records: list[Record] = []
        with open(path, "r", encoding="utf-8") as handle:
            for line_no, line in enumerate(handle, start=1):
                if not line.strip():
                    continue
                payload = json.loads(line)
                self.records.append(_parse_record(payload, path, line_no))
                if max_records is not None and len(self.records) >= max_records:
                    break

    def __len__(self) -> int:
        return len(self.records)

    def __getitem__(self, idx: int) -> Record:
        return self.records[idx]


class SirioNnue2MinimalModel(nn.Module):
    def __init__(self, accumulator_size: int = ACCUMULATOR_SIZE) -> None:
        super().__init__()
        self.input_embedding = nn.Embedding(FEATURES_PER_PERSPECTIVE, accumulator_size)
        self.hidden = nn.Linear(accumulator_size, HIDDEN1_SIZE, bias=True)
        self.output = nn.Linear(HIDDEN1_SIZE, OUTPUT_SIZE, bias=True)

    def forward(self, white_idx: torch.Tensor, black_idx: torch.Tensor) -> torch.Tensor:  # type: ignore[override]
        white_sum = self.input_embedding(white_idx).sum(dim=1)
        black_sum = self.input_embedding(black_idx).sum(dim=1)
        combined = white_sum - black_sum
        hidden = torch.relu(self.hidden(combined))
        return self.output(hidden).squeeze(1)


def _parse_record(payload: dict[str, Any], path: pathlib.Path, line_no: int) -> Record:
    if payload.get("feature_set") != SIRIO_HALFKA_V1_ID:
        raise ValueError(f"{path}:{line_no}: unsupported feature_set '{payload.get('feature_set')}'")

    features = payload.get("features")
    if not isinstance(features, dict):
        raise ValueError(f"{path}:{line_no}: missing features object")

    white = _parse_feature_pairs(features.get("white"), path, line_no, "white")
    black = _parse_feature_pairs(features.get("black"), path, line_no, "black")
    score_cp = float(payload["score_cp"])
    return Record(white_indices=white, black_indices=black, score_cp=score_cp)


def _parse_feature_pairs(raw: Any, path: pathlib.Path, line_no: int, perspective: str) -> list[int]:
    if not isinstance(raw, list):
        raise ValueError(f"{path}:{line_no}: features.{perspective} must be a list")
    indices: list[int] = []
    for pair in raw:
        if not (isinstance(pair, list) and len(pair) == 2):
            raise ValueError(f"{path}:{line_no}: features.{perspective} pair must be [index, value]")
        idx, value = int(pair[0]), int(pair[1])
        if idx < 0 or idx >= FEATURES_PER_PERSPECTIVE:
            raise ValueError(f"{path}:{line_no}: feature index out of range in {perspective}: {idx}")
        if value != 1:
            raise ValueError(f"{path}:{line_no}: feature value must be 1 in {perspective}, got {value}")
        indices.append(idx)
    return indices


def _collate(records: list[Record]) -> tuple[torch.Tensor, torch.Tensor, torch.Tensor]:
    max_white = max((len(r.white_indices) for r in records), default=0)
    max_black = max((len(r.black_indices) for r in records), default=0)
    white = torch.zeros((len(records), max_white), dtype=torch.long)
    black = torch.zeros((len(records), max_black), dtype=torch.long)
    target = torch.tensor([r.score_cp for r in records], dtype=torch.float32)
    for i, rec in enumerate(records):
        if rec.white_indices:
            white[i, : len(rec.white_indices)] = torch.tensor(rec.white_indices, dtype=torch.long)
        if rec.black_indices:
            black[i, : len(rec.black_indices)] = torch.tensor(rec.black_indices, dtype=torch.long)
    return white, black, target


def _set_seed(seed: int) -> None:
    random.seed(seed)
    torch.manual_seed(seed)
    if torch.cuda.is_available():
        torch.cuda.manual_seed_all(seed)
    torch.use_deterministic_algorithms(True, warn_only=True)


def _manifest_digest(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _read_config(path: pathlib.Path) -> dict[str, Any]:
    with open(path, "r", encoding="utf-8") as handle:
        return dict(yaml.safe_load(handle) or {})


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset-dir", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--epochs", type=int, default=1)
    parser.add_argument("--batch-size", type=int, default=8)
    parser.add_argument("--learning-rate", type=float, default=0.001)
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--device", choices=("cpu", "auto"), default="cpu")
    parser.add_argument("--max-records", type=int, default=None)
    parser.add_argument("--config", type=str, default=None)
    return parser


def main() -> None:
    parser = _build_parser()
    args = parser.parse_args()

    if args.config:
        cfg = _read_config(pathlib.Path(args.config))
        for key in ("epochs", "batch_size", "learning_rate", "seed", "device", "max_records"):
            if key in cfg:
                setattr(args, key, cfg[key])

    _set_seed(int(args.seed))
    dataset_dir = pathlib.Path(args.dataset_dir)
    output_dir = pathlib.Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    manifest_path = dataset_dir / "MANIFEST.json"
    train_ds = SparseJsonlDataset(dataset_dir / "train.jsonl", max_records=args.max_records)
    val_ds = SparseJsonlDataset(dataset_dir / "val.jsonl", max_records=args.max_records)

    device = torch.device("cuda" if args.device == "auto" and torch.cuda.is_available() else "cpu")

    model = SirioNnue2MinimalModel().to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=float(args.learning_rate))
    criterion = nn.MSELoss()

    train_loader = DataLoader(train_ds, batch_size=int(args.batch_size), shuffle=False, collate_fn=_collate)
    val_loader = DataLoader(val_ds, batch_size=int(args.batch_size), shuffle=False, collate_fn=_collate)

    logs: list[dict[str, float | int]] = []
    for epoch in range(1, int(args.epochs) + 1):
        model.train()
        train_loss_sum = 0.0
        train_batches = 0
        for white_idx, black_idx, targets in train_loader:
            white_idx, black_idx, targets = white_idx.to(device), black_idx.to(device), targets.to(device)
            optimizer.zero_grad()
            preds = model(white_idx, black_idx)
            loss = criterion(preds, targets)
            loss.backward()
            optimizer.step()
            train_loss_sum += float(loss.item())
            train_batches += 1

        model.eval()
        val_loss_sum = 0.0
        val_batches = 0
        with torch.no_grad():
            for white_idx, black_idx, targets in val_loader:
                white_idx, black_idx, targets = white_idx.to(device), black_idx.to(device), targets.to(device)
                val_loss_sum += float(criterion(model(white_idx, black_idx), targets).item())
                val_batches += 1

        metrics = {
            "epoch": epoch,
            "train_mse": train_loss_sum / max(1, train_batches),
            "val_mse": val_loss_sum / max(1, val_batches),
        }
        logs.append(metrics)
        print(json.dumps(metrics, sort_keys=True))

    metadata = {
        "feature_set": FEATURE_SET,
        "features_per_perspective": FEATURES_PER_PERSPECTIVE,
        "target_contract": "score_cp_white_pov",
        "model_architecture": "shared_sparse_projection(white/black) -> subtract -> Linear/ReLU/Linear -> scalar",
        "dataset_manifest_path": str(manifest_path.resolve()),
        "dataset_manifest_sha256": _manifest_digest(manifest_path),
        "seed": int(args.seed),
        "epochs": int(args.epochs),
        "batch_size": int(args.batch_size),
        "learning_rate": float(args.learning_rate),
        "script_name": SCRIPT_NAME,
        "model_layout_name": MODEL_LAYOUT_NAME,
        "model_layout_version": MODEL_LAYOUT_VERSION,
        "script_version": SCRIPT_VERSION,
        "timestamp": f"deterministic-seed-{int(args.seed)}",
    }
    payload = {
        "state_dict": model.state_dict(),
        "metadata": metadata,
        "model_config": {
            "model_layout_name": MODEL_LAYOUT_NAME,
            "model_layout_version": MODEL_LAYOUT_VERSION,
            "feature_set": FEATURE_SET,
            "features_per_perspective": FEATURES_PER_PERSPECTIVE,
            "accumulator_size": ACCUMULATOR_SIZE,
            "hidden1_size": HIDDEN1_SIZE,
            "hidden2_size": HIDDEN2_SIZE,
            "output_size": OUTPUT_SIZE,
            "activation": ACTIVATION,
            "tensor_order": list(TENSOR_ORDER),
            "quant_input_scale": QUANT_INPUT_SCALE,
            "quant_output_scale": QUANT_OUTPUT_SCALE,
        },
        "logs": logs,
    }
    torch.save(payload, output_dir / "checkpoint.pt")
    (output_dir / "metrics.jsonl").write_text("\n".join(json.dumps(m, sort_keys=True) for m in logs) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
