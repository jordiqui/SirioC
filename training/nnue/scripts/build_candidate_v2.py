"""Build a reproducible CPU-safe SirioNNUE2 minimal candidate artifact bundle."""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from datetime import datetime, timezone
from pathlib import Path

from . import export_to_engine_v2, train_v2
from .nnue2_layout_contract import (
    ACCUMULATOR_SIZE,
    ACTIVATION,
    FEATURE_SET,
    FEATURES_PER_PERSPECTIVE,
    HIDDEN1_SIZE,
    HIDDEN2_SIZE,
    MODEL_LAYOUT_NAME,
    MODEL_LAYOUT_VERSION,
    OUTPUT_SIZE,
)

SCRIPT_VERSION = "p0-34-candidate-v1"


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset-dir", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--epochs", type=int, default=1)
    parser.add_argument("--batch-size", type=int, default=4)
    parser.add_argument("--learning-rate", type=float, default=0.001)
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--device", choices=("cpu", "auto"), default="cpu")
    return parser.parse_args()


def main() -> None:
    args = _parse_args()
    dataset_dir = Path(args.dataset_dir)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    train_args = [
        "--dataset-dir",
        str(dataset_dir),
        "--output-dir",
        str(output_dir),
        "--epochs",
        str(args.epochs),
        "--batch-size",
        str(args.batch_size),
        "--learning-rate",
        str(args.learning_rate),
        "--seed",
        str(args.seed),
        "--device",
        args.device,
    ]
    prev_argv = sys.argv
    try:
        sys.argv = ["train_v2.py", *train_args]
        train_v2.main()
    finally:
        sys.argv = prev_argv

    checkpoint_path = output_dir / "checkpoint.pt"
    candidate_path = output_dir / "candidate.nnue2"
    export_stats = export_to_engine_v2.export(str(candidate_path), str(checkpoint_path))

    dataset_manifest_path = dataset_dir / "MANIFEST.json"
    dataset_manifest_sha = _sha256(dataset_manifest_path) if dataset_manifest_path.exists() else None

    manifest = {
        "script_name": "training.nnue.scripts.build_candidate_v2",
        "script_version": SCRIPT_VERSION,
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "candidate_intent": "test/minimal candidate artifact only; no Elo or strength claim",
        "sirio_nnue2_default_status": "SirioNNUE2 remains non-default",
        "feature_set": FEATURE_SET,
        "features_per_perspective": FEATURES_PER_PERSPECTIVE,
        "model_layout_name": MODEL_LAYOUT_NAME,
        "model_layout_version": MODEL_LAYOUT_VERSION,
        "model_config": {
            "accumulator_size": ACCUMULATOR_SIZE,
            "hidden1_size": HIDDEN1_SIZE,
            "hidden2_size": HIDDEN2_SIZE,
            "output_size": OUTPUT_SIZE,
            "activation": ACTIVATION,
        },
        "training": {
            "epochs": int(args.epochs),
            "batch_size": int(args.batch_size),
            "learning_rate": float(args.learning_rate),
            "seed": int(args.seed),
            "device": args.device,
        },
        "dataset": {
            "dataset_dir": str(dataset_dir.resolve()),
            "manifest_path": str(dataset_manifest_path.resolve()) if dataset_manifest_path.exists() else None,
            "manifest_sha256": dataset_manifest_sha,
        },
        "export": {"mode": export_stats["mode"], "stats": export_stats},
        "artifacts": {
            "checkpoint": {"path": "checkpoint.pt", "sha256": _sha256(checkpoint_path)},
            "candidate": {"path": "candidate.nnue2", "sha256": _sha256(candidate_path)},
        },
        "determinism_note": "checkpoint and candidate are deterministic for fixed dataset/order/seed on CPU-safe settings; generated_at_utc is intentionally non-deterministic.",
    }
    (output_dir / "CANDIDATE_MANIFEST.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    model_card = {
        "name": "SirioNNUE2-MinimalV1 candidate",
        "purpose": "Reproducible artifact contract validation for SirioNNUE2 train/export/runtime path.",
        "status": "Experimental test artifact; no strength claim.",
        "non_default_confirmation": "SirioNNUE2 remains non-default and is not wired as the normal evaluator.",
        "artifacts": ["checkpoint.pt", "candidate.nnue2", "CANDIDATE_MANIFEST.json"],
        "training": manifest["training"],
        "dataset": manifest["dataset"],
    }
    (output_dir / "MODEL_CARD.json").write_text(json.dumps(model_card, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    # Ensure hashes in manifest are stable after writes by re-reading output.
    _read_json(output_dir / "CANDIDATE_MANIFEST.json")


if __name__ == "__main__":
    main()
