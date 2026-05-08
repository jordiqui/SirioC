from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

import torch

ROOT = Path(__file__).resolve().parents[1]
EXPORT_SCRIPT = "training.nnue.scripts.export_to_engine_v2"
TRAIN_SCRIPT = "training.nnue.scripts.train_v2"


def _run(cmd: list[str], *, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=ROOT, check=check, text=True, capture_output=True)


def _write_jsonl(path: Path, rows: list[dict]) -> None:
    path.write_text("\n".join(json.dumps(r, sort_keys=True) for r in rows) + "\n", encoding="utf-8")


def _make_row(feature_set: str = "SirioHalfKAv1") -> dict:
    return {
        "fen": "8/8/8/3k4/8/8/4P3/4K3 w - - 0 1",
        "features": {"white": [[1, 1], [2, 1]], "black": [[3, 1]]},
        "score_cp": 5,
        "result": "1/2-1/2",
        "wdl": 0.5,
        "phase": "middlegame",
        "source": "unit",
        "feature_set": feature_set,
    }


def _make_dataset(dataset_dir: Path, *, feature_set: str = "SirioHalfKAv1") -> None:
    rows = [_make_row(feature_set=feature_set)]
    _write_jsonl(dataset_dir / "train.jsonl", rows)
    _write_jsonl(dataset_dir / "val.jsonl", rows)
    _write_jsonl(dataset_dir / "test.jsonl", rows)
    (dataset_dir / "MANIFEST.json").write_text(json.dumps({"feature_set": feature_set}), encoding="utf-8")


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        t = Path(tmp)
        a = t / "dummy_a.nnue2"
        b = t / "dummy_b.nnue2"

        _run([sys.executable, "-m", EXPORT_SCRIPT, "--output", str(a)])
        _run([sys.executable, "-m", EXPORT_SCRIPT, "--output", str(b)])
        assert a.read_bytes() == b.read_bytes(), "dummy export must remain deterministic"

        sirio_tests = ROOT / "build" / "sirio_tests"
        if sirio_tests.exists():
            _run([str(sirio_tests), "[nnue_roundtrip]"])

        dataset = t / "dataset"
        out = t / "train_out"
        dataset.mkdir()
        _make_dataset(dataset)
        _run(
            [
                sys.executable,
                "-m",
                TRAIN_SCRIPT,
                "--dataset-dir",
                str(dataset),
                "--output-dir",
                str(out),
                "--epochs",
                "1",
                "--batch-size",
                "1",
                "--learning-rate",
                "0.001",
                "--seed",
                "7",
                "--device",
                "cpu",
            ]
        )

        checkpoint = out / "checkpoint.pt"
        failed = _run(
            [sys.executable, "-m", EXPORT_SCRIPT, "--output", str(t / "from_checkpoint.nnue2"), "--checkpoint", str(checkpoint)],
            check=False,
        )
        assert failed.returncode != 0
        assert "mapping safely deferred" in (failed.stderr + failed.stdout)

        bad_feature = out / "bad_feature.pt"
        payload = torch.load(checkpoint, map_location="cpu")
        payload["metadata"]["feature_set"] = "WrongFeature"
        torch.save(payload, bad_feature)
        bad_feature_res = _run(
            [sys.executable, "-m", EXPORT_SCRIPT, "--output", str(t / "bad_feature.nnue2"), "--checkpoint", str(bad_feature)],
            check=False,
        )
        assert bad_feature_res.returncode != 0
        assert "feature_set mismatch" in (bad_feature_res.stderr + bad_feature_res.stdout)

        bad_meta = out / "bad_meta.pt"
        payload2 = torch.load(checkpoint, map_location="cpu")
        payload2.pop("metadata", None)
        torch.save(payload2, bad_meta)
        bad_meta_res = _run(
            [sys.executable, "-m", EXPORT_SCRIPT, "--output", str(t / "bad_meta.nnue2"), "--checkpoint", str(bad_meta)],
            check=False,
        )
        assert bad_meta_res.returncode != 0
        assert "missing metadata" in (bad_meta_res.stderr + bad_meta_res.stdout)

        bad_dims = out / "bad_dims.pt"
        payload3 = torch.load(checkpoint, map_location="cpu")
        payload3["state_dict"]["embedding.weight"] = payload3["state_dict"]["embedding.weight"][:1024, :]
        torch.save(payload3, bad_dims)
        bad_dims_res = _run(
            [sys.executable, "-m", EXPORT_SCRIPT, "--output", str(t / "bad_dims.nnue2"), "--checkpoint", str(bad_dims)],
            check=False,
        )
        assert bad_dims_res.returncode != 0
        assert "incompatible" in (bad_dims_res.stderr + bad_dims_res.stdout)

    print("export v2 bridge checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
