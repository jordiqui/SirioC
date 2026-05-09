from __future__ import annotations

import json
import struct
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


def _assert_header(binary_path: Path) -> None:
    data = binary_path.read_bytes()
    header = struct.unpack("<12sHHHIIIIIIIIIIIII", data[:70])
    magic, version, feature_set_id = header[0], header[1], header[2]
    features_per_perspective, accumulator_size, hidden_dimensions, output_dimensions = (
        header[4],
        header[6],
        header[7],
        header[8],
    )
    assert magic == b"SirioNNUE2\0\0"
    assert version == 2
    assert feature_set_id == 1
    assert features_per_perspective == 40960
    assert accumulator_size == 256
    assert hidden_dimensions == 256
    assert output_dimensions == 1


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        dummy_a = tmp_path / "dummy_a.nnue2"
        dummy_b = tmp_path / "dummy_b.nnue2"

        _run([sys.executable, "-m", EXPORT_SCRIPT, "--output", str(dummy_a)])
        _run([sys.executable, "-m", EXPORT_SCRIPT, "--output", str(dummy_b)])
        assert dummy_a.read_bytes() == dummy_b.read_bytes(), "dummy export must remain deterministic"
        _assert_header(dummy_a)

        dataset = tmp_path / "dataset"
        out = tmp_path / "train_out"
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
        from_checkpoint = tmp_path / "from_checkpoint.nnue2"
        _run([sys.executable, "-m", EXPORT_SCRIPT, "--output", str(from_checkpoint), "--checkpoint", str(checkpoint)])
        _assert_header(from_checkpoint)

        sirio_tests_bin = ROOT / "build" / "sirio_tests"
        assert sirio_tests_bin.exists(), "expected built test binary at build/sirio_tests"
        roundtrip = _run([str(sirio_tests_bin), "[nnue_roundtrip]"])
        assert roundtrip.returncode == 0

        for mutator, error_text in [
            (lambda p: p["metadata"].__setitem__("feature_set", "WrongFeature"), "feature_set mismatch"),
            (lambda p: p.pop("metadata", None), "missing metadata"),
            (lambda p: p.pop("model_config", None), "missing model_config"),
            (lambda p: p.pop("state_dict", None), "missing state_dict"),
            (
                lambda p: p["state_dict"].__setitem__(
                    "input_embedding.weight", p["state_dict"]["input_embedding.weight"][:1024, :]
                ),
                "wrong tensor shape",
            ),
            (lambda p: p["metadata"].__setitem__("model_layout_version", 999), "unsupported model_layout_version"),
        ]:
            bad_payload = torch.load(checkpoint, map_location="cpu")
            mutator(bad_payload)
            bad_checkpoint = tmp_path / f"bad_{error_text[:8]}.pt"
            torch.save(bad_payload, bad_checkpoint)
            failed = _run(
                [
                    sys.executable,
                    "-m",
                    EXPORT_SCRIPT,
                    "--output",
                    str(tmp_path / "bad.nnue2"),
                    "--checkpoint",
                    str(bad_checkpoint),
                ],
                check=False,
            )
            assert failed.returncode != 0
            assert error_text in (failed.stderr + failed.stdout)

    print("export v2 bridge checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
