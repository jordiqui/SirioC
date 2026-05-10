from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD_CANDIDATE_SCRIPT = "training.nnue.scripts.build_candidate_v2"


def _sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _run(cmd: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=ROOT, check=True, text=True, capture_output=True)


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


def _write_jsonl(path: Path, rows: list[dict]) -> None:
    path.write_text("\n".join(json.dumps(r, sort_keys=True) for r in rows) + "\n", encoding="utf-8")


def _build_dataset(path: Path) -> None:
    rows = [
        _row("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 12, [1, 2, 3], [4, 5]),
        _row("8/8/8/8/8/8/6k1/6K1 w - - 0 1", 0, [10], [11]),
        _row("4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1", -44, [42, 43], [44, 45, 46]),
    ]
    _write_jsonl(path / "train.jsonl", rows)
    _write_jsonl(path / "val.jsonl", rows[:2])
    _write_jsonl(path / "test.jsonl", rows[1:])
    (path / "MANIFEST.json").write_text(
        json.dumps({"feature_set": "SirioHalfKAv1", "contract": "candidate-v2-artifact-test"}, sort_keys=True),
        encoding="utf-8",
    )


def _assert_format_detection(candidate_path: Path) -> None:
    smoke_bin = ROOT / "build" / "sirio_nnue_runtime_smoke_contract"
    assert smoke_bin.exists(), "expected build/sirio_nnue_runtime_smoke_contract"
    run = _run([str(smoke_bin), str(candidate_path)])
    assert run.returncode == 0


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        dataset_dir = tmp_path / "dataset_v2"
        out1 = tmp_path / "out1"
        out2 = tmp_path / "out2"
        dataset_dir.mkdir()
        _build_dataset(dataset_dir)

        base_cmd = [
            sys.executable,
            "-m",
            BUILD_CANDIDATE_SCRIPT,
            "--dataset-dir",
            str(dataset_dir),
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
        _run([*base_cmd, "--output-dir", str(out1)])
        _run([*base_cmd, "--output-dir", str(out2)])

        checkpoint = out1 / "checkpoint.pt"
        candidate = out1 / "candidate.nnue2"
        manifest = out1 / "CANDIDATE_MANIFEST.json"
        model_card = out1 / "MODEL_CARD.json"
        assert checkpoint.exists()
        assert candidate.exists()
        assert manifest.exists()
        assert model_card.exists()

        payload = json.loads(manifest.read_text(encoding="utf-8"))
        assert payload["feature_set"] == "SirioHalfKAv1"
        assert payload["features_per_perspective"] == 40960
        assert payload["model_layout_name"] == "SirioNNUE2-MinimalV1"
        assert payload["model_layout_version"] == 1
        assert payload["training"]["seed"] == 2025
        assert "sha256" in payload["artifacts"]["checkpoint"]
        assert "sha256" in payload["artifacts"]["candidate"]
        assert "no Elo or strength claim" in payload["candidate_intent"]

        assert payload["artifacts"]["checkpoint"]["sha256"] == _sha(checkpoint)
        assert payload["artifacts"]["candidate"]["sha256"] == _sha(candidate)
        assert payload["dataset"]["manifest_sha256"] == _sha(dataset_dir / "MANIFEST.json")

        _assert_format_detection(candidate)

        assert _sha(out1 / "checkpoint.pt") == _sha(out2 / "checkpoint.pt")
        assert _sha(out1 / "candidate.nnue2") == _sha(out2 / "candidate.nnue2")

    print("nnue candidate v2 artifact test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
