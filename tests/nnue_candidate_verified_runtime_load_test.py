from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD_CANDIDATE_SCRIPT = "training.nnue.scripts.build_candidate_v2"
VERIFIED_RUNTIME_LOAD_SCRIPT = "training.nnue.scripts.verified_runtime_load_v2"


def _run(cmd: list[str], check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=ROOT, check=check, text=True, capture_output=True)


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
        json.dumps({"feature_set": "SirioHalfKAv1", "contract": "candidate-verified-runtime-load-test"}, sort_keys=True),
        encoding="utf-8",
    )


def _run_verified_load(candidate_dir: Path, check: bool = True) -> subprocess.CompletedProcess[str]:
    return _run([sys.executable, "-m", VERIFIED_RUNTIME_LOAD_SCRIPT, "--candidate-dir", str(candidate_dir)], check=check)


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        dataset_dir = tmp_path / "dataset_v2"
        out = tmp_path / "out"
        dataset_dir.mkdir()
        _build_dataset(dataset_dir)

        _run(
            [
                sys.executable,
                "-m",
                BUILD_CANDIDATE_SCRIPT,
                "--dataset-dir",
                str(dataset_dir),
                "--output-dir",
                str(out),
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

        ok = _run_verified_load(out)
        ok_payload = json.loads(ok.stdout)
        assert ok_payload["verification_attempted"] is True
        assert ok_payload["verification_succeeded"] is True
        assert ok_payload["load_attempted"] is True
        assert ok_payload["load_succeeded"] is True
        assert ok_payload["failure_reason"] == ""

        candidate = out / "candidate.nnue2"
        original = candidate.read_bytes()
        candidate.write_bytes(original + b"corrupt")
        failed_corrupt = _run_verified_load(out, check=False)
        assert failed_corrupt.returncode != 0
        failed_corrupt_payload = json.loads(failed_corrupt.stdout)
        assert failed_corrupt_payload["verification_succeeded"] is False
        assert failed_corrupt_payload["load_attempted"] is False
        candidate.write_bytes(original)

        manifest = out / "CANDIDATE_MANIFEST.json"
        manifest_json = json.loads(manifest.read_text(encoding="utf-8"))
        manifest_json["artifacts"]["candidate"]["sha256"] = "0" * 64
        manifest.write_text(json.dumps(manifest_json, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        failed_manifest = _run_verified_load(out, check=False)
        assert failed_manifest.returncode != 0
        failed_manifest_payload = json.loads(failed_manifest.stdout)
        assert failed_manifest_payload["verification_succeeded"] is False
        assert failed_manifest_payload["load_attempted"] is False

        model_card = out / "MODEL_CARD.json"
        model_card.unlink()
        failed_missing_model = _run_verified_load(out, check=False)
        assert failed_missing_model.returncode != 0
        failed_missing_model_payload = json.loads(failed_missing_model.stdout)
        assert failed_missing_model_payload["verification_succeeded"] is False
        assert failed_missing_model_payload["load_attempted"] is False

        fake_dir = tmp_path / "fake_stockfish_candidate"
        fake_dir.mkdir()
        (fake_dir / "candidate.nnue2").write_bytes(b"stockfish_nnue_payload")
        (fake_dir / "checkpoint.pt").write_bytes(b"dummy-checkpoint")
        (fake_dir / "CANDIDATE_MANIFEST.json").write_text(manifest.read_text(encoding="utf-8"), encoding="utf-8")
        (fake_dir / "MODEL_CARD.json").write_text(
            json.dumps(
                {
                    "status": "Experimental artifact only; no strength claim.",
                    "non_default_confirmation": "SirioNNUE2 remains non-default and is not wired as the normal evaluator.",
                    "training": manifest_json["training"],
                    "dataset": manifest_json["dataset"],
                },
                sort_keys=True,
            ),
            encoding="utf-8",
        )
        failed_fake = _run_verified_load(fake_dir, check=False)
        assert failed_fake.returncode != 0
        failed_fake_payload = json.loads(failed_fake.stdout)
        assert failed_fake_payload["verification_succeeded"] is False
        assert failed_fake_payload["load_attempted"] is False

    print("nnue candidate verified runtime load test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
