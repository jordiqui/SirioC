"""Dataset utilities for training the Sirio NNUE."""
from __future__ import annotations

import json
import math
import pathlib
from dataclasses import dataclass
from typing import Tuple

import numpy as np
import torch
from torch.utils.data import Dataset, Subset


@dataclass
class DatasetMetadata:
    """Metadata describing how a dataset was produced."""

    name: str
    samples: int
    seed: int
    source: list[str]
    sample_stride: int
    result_weight: float
    target: str

    @staticmethod
    def from_file(path: str | pathlib.Path) -> "DatasetMetadata":
        with open(path, "r", encoding="utf-8") as handle:
            raw = json.load(handle)
        return DatasetMetadata(
            name=raw["name"],
            samples=raw["samples"],
            seed=raw["seed"],
            source=list(raw["source"]),
            sample_stride=raw["sample_stride"],
            result_weight=float(raw["result_weight"]),
            target=raw["target"],
        )


class PieceCountDataset(Dataset[Tuple[torch.Tensor, torch.Tensor]]):
    """PyTorch Dataset wrapper around the `.npz` files created by the pipeline."""

    def __init__(self, npz_path: str | pathlib.Path) -> None:
        archive = np.load(npz_path)
        self.features = torch.from_numpy(archive["features"]).float()
        self.targets = torch.from_numpy(archive["targets"]).float()
        self.ply = torch.from_numpy(archive["ply"]).long()

    def __len__(self) -> int:  # type: ignore[override]
        return self.features.shape[0]

    def __getitem__(self, index: int) -> Tuple[torch.Tensor, torch.Tensor]:  # type: ignore[override]
        return self.features[index], self.targets[index]


def deterministic_split(dataset: Dataset, validation_split: float, seed: int) -> Tuple[Subset, Subset]:
    """Split the dataset into train/validation subsets deterministically."""

    if not 0.0 < validation_split < 1.0:
        raise ValueError("validation_split must be in (0, 1)")

    total = len(dataset)
    indices = np.arange(total)
    rng = np.random.default_rng(seed)
    rng.shuffle(indices)

    val_size = int(math.floor(total * validation_split))
    val_indices = indices[:val_size]
    train_indices = indices[val_size:]

    return Subset(dataset, train_indices.tolist()), Subset(dataset, val_indices.tolist())


