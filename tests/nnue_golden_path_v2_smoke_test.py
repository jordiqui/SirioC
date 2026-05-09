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


START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
KINGS_ONLY_FEN = "8/8/8/8/8/8/6k1/6K1 w - - 0 1"
ASYMMETRIC_FEN = "4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1"


def _run(cmd: list[str], *, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=ROOT, check=check, text=True, capture_output=True)


def _write_jsonl(path: Path, rows: list[dict]) -> None:
    path.write_text("\n".join(json.dumps(row, sort_keys=True) for row in rows) + "\n", encoding="utf-8")


def _row(fen: str, score_cp: int, white_idx: list[int], black_idx: list[int]) -> dict:
    return {
        "fen": fen,
        "features": {
            "white": [[idx, 1] for idx in white_idx],
            "black": [[idx, 1] for idx in black_idx],
        },
        "score_cp": score_cp,
        "result": "1/2-1/2",
        "wdl": 0.5,
        "phase": "middlegame",
        "source": "unit",
        "feature_set": "SirioHalfKAv1",
    }


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        dataset_dir = tmp_path / "dataset_v2"
        output_dir = tmp_path / "train_out"
        checkpoint_path = output_dir / "checkpoint.pt"
        network_path = tmp_path / "golden_path.nnue2"
        dataset_dir.mkdir()

        rows = [
            _row(START_FEN, 12, [1, 2, 3], [4, 5]),
            _row(KINGS_ONLY_FEN, 0, [10], [11]),
            _row(ASYMMETRIC_FEN, -44, [42, 43], [44, 45, 46]),
        ]
        _write_jsonl(dataset_dir / "train.jsonl", rows)
        _write_jsonl(dataset_dir / "val.jsonl", rows[:2])
        _write_jsonl(dataset_dir / "test.jsonl", rows[1:])
        (dataset_dir / "MANIFEST.json").write_text(
            json.dumps({"feature_set": "SirioHalfKAv1", "contract": "golden-path-smoke"}, sort_keys=True),
            encoding="utf-8",
        )

        _run(
            [
                sys.executable,
                "-m",
                TRAIN_SCRIPT,
                "--dataset-dir",
                str(dataset_dir),
                "--output-dir",
                str(output_dir),
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
        assert checkpoint_path.exists(), "train_v2 checkpoint missing"

        payload = torch.load(checkpoint_path, map_location="cpu")
        md = payload["metadata"]
        assert md["script_name"] == "training.nnue.scripts.train_v2"
        assert md["feature_set"] == "SirioHalfKAv1"
        assert md["features_per_perspective"] == 40960
        assert md["model_layout_name"] == "SirioNNUE2-MinimalV1"
        assert md["model_layout_version"] == 1

        _run([
            sys.executable,
            "-m",
            EXPORT_SCRIPT,
            "--checkpoint",
            str(checkpoint_path),
            "--output",
            str(network_path),
        ])
        assert network_path.exists(), "exported SirioNNUE2 binary missing"

        smoke_bin = ROOT / "build" / "sirio_nnue_runtime_smoke_contract"
        assert smoke_bin.exists(), "expected build/sirio_nnue_runtime_smoke_contract"

        smoke_run = _run([str(smoke_bin), str(network_path)])
        assert smoke_run.returncode == 0

    print("nnue2 golden path smoke test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
