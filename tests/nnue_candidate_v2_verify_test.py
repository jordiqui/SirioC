from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD_CANDIDATE_SCRIPT = "training.nnue.scripts.build_candidate_v2"
VERIFY_CANDIDATE_SCRIPT = "training.nnue.scripts.verify_candidate_v2"


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
        json.dumps({"feature_set": "SirioHalfKAv1", "contract": "candidate-v2-verify-test"}, sort_keys=True),
        encoding="utf-8",
    )


def _sha256(path: Path) -> str:
    import hashlib

    return hashlib.sha256(path.read_bytes()).hexdigest()


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

        report_path = tmp_path / "verify_report.json"
        verify_run = _run(
            [
                sys.executable,
                "-m",
                VERIFY_CANDIDATE_SCRIPT,
                "--candidate-dir",
                str(out),
                "--report",
                str(report_path),
            ]
        )
        payload = json.loads(verify_run.stdout)
        assert payload["success"] is True
        assert payload["format"] == "SirioNNUE2MinimalV1"
        assert payload["hash_checks_passed"] is True
        assert payload["non_default_confirmed"] is True
        assert payload["no_strength_claim_confirmed"] is True
        assert json.loads(report_path.read_text(encoding="utf-8"))["success"] is True

        candidate = out / "candidate.nnue2"
        backup_candidate = candidate.read_bytes()
        candidate.write_bytes(backup_candidate + b"corrupt")
        failed_corrupt = _run([sys.executable, "-m", VERIFY_CANDIDATE_SCRIPT, "--candidate-dir", str(out)], check=False)
        assert failed_corrupt.returncode != 0
        assert "sha256 mismatch" in failed_corrupt.stdout
        candidate.write_bytes(backup_candidate)

        manifest = out / "CANDIDATE_MANIFEST.json"
        manifest_json = json.loads(manifest.read_text(encoding="utf-8"))
        manifest_json["artifacts"]["candidate"]["sha256"] = "0" * 64
        manifest.write_text(json.dumps(manifest_json, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        failed_manifest = _run([sys.executable, "-m", VERIFY_CANDIDATE_SCRIPT, "--candidate-dir", str(out)], check=False)
        assert failed_manifest.returncode != 0
        assert "candidate.nnue2 sha256 mismatch" in failed_manifest.stdout

        model_card = out / "MODEL_CARD.json"
        model_card.unlink()
        failed_missing_model_card = _run(
            [sys.executable, "-m", VERIFY_CANDIDATE_SCRIPT, "--candidate-dir", str(out)],
            check=False,
        )
        assert failed_missing_model_card.returncode != 0
        assert "missing required file" in failed_missing_model_card.stdout

        fake_dir = tmp_path / "fake_stockfish_candidate"
        fake_dir.mkdir()
        (fake_dir / "candidate.nnue2").write_bytes(b"stockfish_nnue_payload")
        (fake_dir / "checkpoint.pt").write_bytes(b"dummy-checkpoint")
        fake_candidate = fake_dir / "candidate.nnue2"
        fake_checkpoint = fake_dir / "checkpoint.pt"
        (fake_dir / "CANDIDATE_MANIFEST.json").write_text(
            json.dumps(
                {
                    "feature_set": "SirioHalfKAv1",
                    "features_per_perspective": 40960,
                    "model_layout_name": "SirioNNUE2-MinimalV1",
                    "model_layout_version": 1,
                    "candidate_intent": "test/minimal candidate artifact only; no Elo or strength claim",
                    "sirio_nnue2_default_status": "SirioNNUE2 remains non-default",
                    "training": {"seed": 1},
                    "dataset": {"manifest_sha256": None},
                    "artifacts": {
                        "candidate": {"path": "candidate.nnue2", "sha256": _sha256(fake_candidate)},
                        "checkpoint": {"path": "checkpoint.pt", "sha256": _sha256(fake_checkpoint)},
                    },
                },
                sort_keys=True,
            ),
            encoding="utf-8",
        )
        (fake_dir / "MODEL_CARD.json").write_text(
            json.dumps(
                {
                    "status": "Experimental test artifact; no strength claim.",
                    "non_default_confirmation": "SirioNNUE2 remains non-default and is not wired as the normal evaluator.",
                    "training": {"seed": 1},
                    "dataset": {"manifest_sha256": None},
                },
                sort_keys=True,
            ),
            encoding="utf-8",
        )
        failed_fake = _run([sys.executable, "-m", VERIFY_CANDIDATE_SCRIPT, "--candidate-dir", str(fake_dir)], check=False)
        assert failed_fake.returncode != 0
        assert "format mismatch" in failed_fake.stdout

    print("nnue candidate v2 verify test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
