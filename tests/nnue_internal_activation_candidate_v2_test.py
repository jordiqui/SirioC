from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD_CANDIDATE_SCRIPT = "training.nnue.scripts.build_candidate_v2"
VERIFY_SCRIPT = "training.nnue.scripts.verify_candidate_v2"
VERIFIED_RUNTIME_LOAD_SCRIPT = "training.nnue.scripts.verified_runtime_load_v2"


def _run(cmd: list[str], check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=ROOT, check=check, text=True, capture_output=True)


def _row(fen: str, score_cp: int, white_idx: list[int], black_idx: list[int]) -> dict:
    return {
        "fen": fen,
        "features": {"white": [[idx, 1] for idx in white_idx], "black": [[idx, 1] for idx in black_idx]},
        "score_cp": score_cp,
        "result": "1/2-1/2",
        "wdl": 0.5,
        "phase": "middlegame",
        "source": "unit",
        "feature_set": "SirioHalfKAv1",
    }


def _build_dataset(path: Path) -> None:
    rows = [
        _row("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 12, [1, 2, 3], [4, 5]),
        _row("8/8/8/8/8/8/6k1/6K1 w - - 0 1", 0, [10], [11]),
        _row("4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1", -44, [42, 43], [44, 45, 46]),
    ]
    for split_name, split_rows in (("train", rows), ("val", rows[:2]), ("test", rows[1:])):
        (path / f"{split_name}.jsonl").write_text(
            "\n".join(json.dumps(r, sort_keys=True) for r in split_rows) + "\n", encoding="utf-8"
        )
    (path / "MANIFEST.json").write_text(
        json.dumps({"feature_set": "SirioHalfKAv1", "contract": "internal-activation-candidate-v2"}, sort_keys=True),
        encoding="utf-8",
    )


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        dataset_dir = tmp_path / "dataset_v2"
        out_dir = tmp_path / "out"
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
                str(out_dir),
                "--epochs",
                "1",
                "--batch-size",
                "2",
                "--learning-rate",
                "0.001",
                "--seed",
                "2042",
                "--device",
                "cpu",
            ]
        )

        candidate = out_dir / "candidate.nnue2"
        fixture_network = tmp_path / "fixture_runtime.bin"
        _run([sys.executable, "-m", "training.nnue.scripts.export_to_engine_v2", "--output", str(fixture_network)])

        verified = _run([sys.executable, "-m", VERIFY_SCRIPT, "--candidate-dir", str(out_dir)])
        assert verified.returncode == 0

        verified_runtime = _run(
            [sys.executable, "-m", VERIFIED_RUNTIME_LOAD_SCRIPT, "--candidate-dir", str(out_dir)]
        )
        verified_runtime_payload = json.loads(verified_runtime.stdout)
        assert verified_runtime_payload["verification_succeeded"] is True
        assert verified_runtime_payload["load_succeeded"] is True

        helper = ROOT / "build" / "sirio_nnue_internal_activation_contract"
        assert helper.exists(), "expected build/sirio_nnue_internal_activation_contract"
        candidate_attempt = _run([str(helper), str(candidate)], check=False)
        assert candidate_attempt.returncode != 0
        assert "LOAD_REJECTED|" in candidate_attempt.stdout

        good = _run([str(helper), str(fixture_network)], check=False)
        assert good.returncode != 0
        assert "LOAD_REJECTED|" in good.stdout

        bad_dir = tmp_path / "unverified"
        bad_dir.mkdir()
        bad_candidate = bad_dir / "candidate.nnue2"
        bad_candidate.write_bytes(candidate.read_bytes() + b"tamper")
        bad = _run([str(helper), str(bad_candidate)], check=False)
        assert bad.returncode != 0
        assert "LOAD_REJECTED|" in bad.stdout

        missing = _run([str(helper), str(tmp_path / "missing.nnue2")], check=False)
        assert missing.returncode != 0
        assert "LOAD_REJECTED|" in missing.stdout

    print("nnue internal activation candidate v2 test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
