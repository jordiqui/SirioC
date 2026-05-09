from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

import torch

ROOT = Path(__file__).resolve().parents[1]
TRAIN_SCRIPT = "training.nnue.scripts.train_v2"
EXPORT_SCRIPT = "training.nnue.scripts.export_to_engine_v2"


def _run(cmd: list[str], *, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=ROOT, check=check, text=True, capture_output=True)


def _write_jsonl(path: Path, rows: list[dict]) -> None:
    path.write_text("\n".join(json.dumps(r, sort_keys=True) for r in rows) + "\n", encoding="utf-8")


def _row(fen: str, score_cp: int, white_idx: list[int], black_idx: list[int]) -> dict:
    return {
        "fen": fen,
        "features": {"white": [[i, 1] for i in white_idx], "black": [[i, 1] for i in black_idx]},
        "score_cp": score_cp,
        "result": "1/2-1/2",
        "wdl": 0.5,
        "phase": "middlegame",
        "source": "p0-25-smoke",
        "feature_set": "SirioHalfKAv1",
    }


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        dataset_dir = tmp_path / "dataset"
        train_out = tmp_path / "train_out"
        export_path = tmp_path / "tiny.nnue2"
        dataset_dir.mkdir()

        rows = [
            _row("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 15, [1, 2, 3], [7, 8, 9]),
            _row("8/8/8/8/8/8/6k1/6K1 w - - 0 1", 0, [10], [11]),
            _row("4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1", 27, [301, 302], [401]),
        ]
        _write_jsonl(dataset_dir / "train.jsonl", rows)
        _write_jsonl(dataset_dir / "val.jsonl", rows[:2])
        _write_jsonl(dataset_dir / "test.jsonl", rows[1:])
        (dataset_dir / "MANIFEST.json").write_text(json.dumps({"feature_set": "SirioHalfKAv1"}, sort_keys=True), encoding="utf-8")

        _run(
            [
                sys.executable,
                "-m",
                TRAIN_SCRIPT,
                "--dataset-dir",
                str(dataset_dir),
                "--output-dir",
                str(train_out),
                "--epochs",
                "1",
                "--batch-size",
                "2",
                "--learning-rate",
                "0.001",
                "--seed",
                "2025",
                "--device",
                "cpu",
            ]
        )

        checkpoint = train_out / "checkpoint.pt"
        assert checkpoint.exists(), "checkpoint must be created"

        payload = torch.load(checkpoint, map_location="cpu")
        metadata = payload["metadata"]
        assert metadata["script_name"] == "training.nnue.scripts.train_v2"
        assert metadata["feature_set"] == "SirioHalfKAv1"
        assert metadata["features_per_perspective"] == 40960
        assert metadata["model_layout_name"] == "SirioNNUE2-MinimalV1"
        assert metadata["model_layout_version"] == 1

        _run([sys.executable, "-m", EXPORT_SCRIPT, "--checkpoint", str(checkpoint), "--output", str(export_path)])
        assert export_path.exists(), "exported .nnue2 file must be created"

        smoke_bin = ROOT / "build" / "sirio_nnue_runtime_smoke_contract"
        assert smoke_bin.exists(), "expected built smoke contract helper at build/sirio_nnue_runtime_smoke_contract"
        smoke = _run([str(smoke_bin), str(export_path)])
        assert smoke.returncode == 0

    print("train-export-runtime golden path smoke contract passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
