from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))


def _read_jsonl(path: Path) -> list[dict]:
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        input_path = tmp_path / "records.tsv"
        output_dir = tmp_path / "out"

        input_path.write_text(
            "\n".join(
                [
                    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1\t15\t1-0\tunit",
                    "8/8/8/8/8/8/4k3/4K3 w - - 0 1\t-2\t1/2-1/2\tunit",
                    "8/8/8/8/8/8/8/4K3 w - - 0 1\t33\t0-1\tbad",
                    "not-a-fen\t12\t*\tbad",
                ]
            ),
            encoding="utf-8",
        )

        cmd = [
            sys.executable,
            "-m",
            "training.nnue.scripts.prepare_dataset_v2",
            "--input",
            str(input_path),
            "--output-dir",
            str(output_dir),
            "--format",
            "tsv",
            "--seed",
            "99",
            "--val-ratio",
            "0.25",
            "--test-ratio",
            "0.25",
            "--source",
            "fallback",
        ]
        subprocess.run(cmd, check=True, cwd=ROOT)

        manifest = json.loads((output_dir / "MANIFEST.json").read_text(encoding="utf-8"))
        assert manifest["feature_set"] == "SirioHalfKAv1"
        assert manifest["score_cp_perspective"] == "white"
        assert manifest["split_counts"] == {"train": 2, "val": 0, "test": 0} or sum(manifest["split_counts"].values()) == 2
        assert manifest["accepted_records"] == 2
        assert manifest["rejected_records"] == 2

        train_rows = _read_jsonl(output_dir / "train.jsonl")
        val_rows = _read_jsonl(output_dir / "val.jsonl")
        test_rows = _read_jsonl(output_dir / "test.jsonl")
        all_rows = train_rows + val_rows + test_rows

        assert len(all_rows) == 2
        fens = {row["fen"] for row in all_rows}
        start = next(row for row in all_rows if row["fen"].startswith("rnbqkbnr"))
        kings = next(row for row in all_rows if row["fen"].startswith("8/8/8/8/8/8/4k3/4K3"))

        assert len(start["features"]["white"]) == 30
        assert len(start["features"]["black"]) == 30
        assert len(kings["features"]["white"]) == 0
        assert len(kings["features"]["black"]) == 0

        assert start["score_cp"] == 15
        assert kings["score_cp"] == -2
        assert start["wdl"] == 1.0
        assert kings["wdl"] == 0.5

        for row in all_rows:
            seen = set()
            for persp in ("white", "black"):
                for idx, value in row["features"][persp]:
                    assert 0 <= idx < 40960
                    assert value == 1
                    seen.add((persp, idx))

        output_dir2 = tmp_path / "out2"
        cmd2 = cmd.copy()
        cmd2[cmd2.index("--output-dir") + 1] = str(output_dir2)
        subprocess.run(cmd2, check=True, cwd=ROOT)
        rows2 = _read_jsonl(output_dir2 / "train.jsonl") + _read_jsonl(output_dir2 / "val.jsonl") + _read_jsonl(output_dir2 / "test.jsonl")
        assert [r["fen"] for r in all_rows] == [r["fen"] for r in rows2]
        assert fens == {r["fen"] for r in rows2}

    print("dataset v2 checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
