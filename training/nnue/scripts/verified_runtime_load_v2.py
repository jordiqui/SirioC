"""Verified-only SirioNNUE2 runtime load contract for internal/test use."""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

VERIFY_SCRIPT = "training.nnue.scripts.verify_candidate_v2"


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate-dir", required=True)
    parser.add_argument("--runtime-smoke-bin")
    parser.add_argument("--report")
    return parser.parse_args()


def main() -> int:
    args = _parse_args()
    candidate_dir = Path(args.candidate_dir)
    candidate_path = candidate_dir / "candidate.nnue2"
    manifest_path = candidate_dir / "CANDIDATE_MANIFEST.json"

    report = {
        "verification_attempted": True,
        "verification_succeeded": False,
        "load_attempted": False,
        "load_succeeded": False,
        "failure_reason": "",
        "candidate_path": str(candidate_path),
        "manifest_path": str(manifest_path),
    }

    verify_run = subprocess.run(
        [sys.executable, "-m", VERIFY_SCRIPT, "--candidate-dir", str(candidate_dir)],
        text=True,
        capture_output=True,
        check=False,
    )
    if verify_run.returncode != 0:
        report["failure_reason"] = "candidate verification failed before runtime load"
        report["verification_output"] = verify_run.stdout
        print(json.dumps(report, indent=2, sort_keys=True))
        if args.report:
            Path(args.report).write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        return 1

    report["verification_succeeded"] = True
    report["load_attempted"] = True

    smoke_bin = Path(args.runtime_smoke_bin) if args.runtime_smoke_bin else Path(__file__).resolve().parents[3] / "build" / "sirio_nnue_runtime_smoke_contract"
    run = subprocess.run([str(smoke_bin), str(candidate_path)], text=True, capture_output=True, check=False)
    if run.returncode != 0:
        report["failure_reason"] = "runtime load rejected after verification"
        report["runtime_output"] = (run.stdout + "\n" + run.stderr).strip()
        print(json.dumps(report, indent=2, sort_keys=True))
        if args.report:
            Path(args.report).write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        return 1

    report["load_succeeded"] = True
    print(json.dumps(report, indent=2, sort_keys=True))
    if args.report:
        Path(args.report).write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
