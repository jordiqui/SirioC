from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

import torch

ROOT = Path(__file__).resolve().parents[1]


def _write_jsonl(path: Path, rows: list[dict]) -> None:
    path.write_text("\n".join(json.dumps(r, sort_keys=True) for r in rows) + "\n", encoding="utf-8")


def _make_row(fen: str, score_cp: int, white_idx: list[int], black_idx: list[int], feature_set: str = "SirioHalfKAv1") -> dict:
    return {
        "fen": fen,
        "features": {"white": [[i, 1] for i in white_idx], "black": [[i, 1] for i in black_idx]},
        "score_cp": score_cp,
        "result": "1/2-1/2",
        "wdl": 0.5,
        "phase": "middlegame",
        "source": "unit",
        "feature_set": feature_set,
    }


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        dataset_dir = tmp_path / "dataset"
        out1 = tmp_path / "out1"
        out2 = tmp_path / "out2"
        dataset_dir.mkdir()

        rows = [
            _make_row("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 20, [1, 2, 3], [4, 5, 6]),
            _make_row("8/8/8/3k4/8/8/4P3/4K3 w - - 0 1", -15, [11, 12], [100]),
            _make_row("4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1", 33, [88], [77, 66]),
        ]
        _write_jsonl(dataset_dir / "train.jsonl", rows)
        _write_jsonl(dataset_dir / "val.jsonl", rows[:1])
        _write_jsonl(dataset_dir / "test.jsonl", rows[:1])
        (dataset_dir / "MANIFEST.json").write_text(json.dumps({"feature_set": "SirioHalfKAv1"}, sort_keys=True), encoding="utf-8")

        cmd = [
            sys.executable,
            "-m",
            "training.nnue.scripts.train_v2",
            "--dataset-dir",
            str(dataset_dir),
            "--output-dir",
            str(out1),
            "--epochs",
            "2",
            "--batch-size",
            "2",
            "--learning-rate",
            "0.001",
            "--seed",
            "123",
            "--device",
            "cpu",
        ]
        subprocess.run(cmd, check=True, cwd=ROOT)
        cmd2 = cmd.copy()
        cmd2[cmd2.index("--output-dir") + 1] = str(out2)
        subprocess.run(cmd2, check=True, cwd=ROOT)

        ckpt1 = out1 / "checkpoint.pt"
        ckpt2 = out2 / "checkpoint.pt"
        assert ckpt1.exists()
        assert ckpt2.exists()

        payload1 = torch.load(ckpt1, map_location="cpu")
        payload2 = torch.load(ckpt2, map_location="cpu")
        md = payload1["metadata"]
        assert md["feature_set"] == "SirioHalfKAv1"
        assert md["features_per_perspective"] == 40960
        assert md["seed"] == 123
        assert md["timestamp"] == "deterministic-seed-123"
        assert payload1["metadata"] == payload2["metadata"]

        bad_feature_set = dataset_dir / "bad_feature_set"
        bad_feature_set.mkdir()
        bad_rows = [dict(rows[0], feature_set="Wrong")]
        _write_jsonl(bad_feature_set / "train.jsonl", bad_rows)
        _write_jsonl(bad_feature_set / "val.jsonl", bad_rows)
        _write_jsonl(bad_feature_set / "test.jsonl", bad_rows)
        (bad_feature_set / "MANIFEST.json").write_text("{}", encoding="utf-8")
        bad_cmd = cmd.copy()
        bad_cmd[bad_cmd.index("--dataset-dir") + 1] = str(bad_feature_set)
        bad_cmd[bad_cmd.index("--output-dir") + 1] = str(tmp_path / "bad_out")
        failed = subprocess.run(bad_cmd, cwd=ROOT, capture_output=True, text=True)
        assert failed.returncode != 0
        assert "unsupported feature_set" in (failed.stderr + failed.stdout)

        bad_index = dataset_dir / "bad_index"
        bad_index.mkdir()
        oob = [_make_row(rows[0]["fen"], 1, [50000], [1])]
        _write_jsonl(bad_index / "train.jsonl", oob)
        _write_jsonl(bad_index / "val.jsonl", oob)
        _write_jsonl(bad_index / "test.jsonl", oob)
        (bad_index / "MANIFEST.json").write_text("{}", encoding="utf-8")
        bad_cmd2 = cmd.copy()
        bad_cmd2[bad_cmd2.index("--dataset-dir") + 1] = str(bad_index)
        bad_cmd2[bad_cmd2.index("--output-dir") + 1] = str(tmp_path / "bad_out2")
        failed2 = subprocess.run(bad_cmd2, cwd=ROOT, capture_output=True, text=True)
        assert failed2.returncode != 0
        assert "feature index out of range" in (failed2.stderr + failed2.stdout)

    print("train v2 checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
