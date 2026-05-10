"""Verify SirioNNUE2 candidate artifacts against manifest/model-card integrity contracts."""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

from .nnue2_layout_contract import FEATURE_SET, FEATURES_PER_PERSPECTIVE, MODEL_LAYOUT_NAME, MODEL_LAYOUT_VERSION


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _contains_no_strength_claim(text: str) -> bool:
    lowered = text.lower()
    return "no elo" in lowered or "no strength claim" in lowered


def _contains_non_default(text: str) -> bool:
    lowered = text.lower()
    return "non-default" in lowered or "remains non-default" in lowered


def _detect_format_with_helper(candidate_path: Path, helper_bin: Path | None) -> tuple[str, str]:
    if helper_bin is None:
        helper_bin = Path(__file__).resolve().parents[3] / "build" / "sirio_nnue_format_detect_contract"
    run = subprocess.run(
        [str(helper_bin), str(candidate_path)],
        text=True,
        capture_output=True,
        check=False,
    )
    if run.returncode != 0:
        raise RuntimeError(f"format helper failed: {run.stderr.strip() or run.stdout.strip() or 'unknown error'}")
    payload = json.loads(run.stdout)
    return str(payload.get("format", "Unknown")), str(payload.get("diagnostic", ""))


def _runtime_smoke_format_check(candidate_path: Path) -> bool:
    smoke_bin = Path(__file__).resolve().parents[3] / "build" / "sirio_nnue_runtime_smoke_contract"
    run = subprocess.run([str(smoke_bin), str(candidate_path)], text=True, capture_output=True, check=False)
    return run.returncode == 0


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate-dir", required=True)
    parser.add_argument("--candidate")
    parser.add_argument("--manifest")
    parser.add_argument("--model-card")
    parser.add_argument("--checkpoint")
    parser.add_argument("--format-helper")
    parser.add_argument("--report")
    return parser.parse_args()


def main() -> int:
    args = _parse_args()
    candidate_dir = Path(args.candidate_dir)
    candidate_path = Path(args.candidate) if args.candidate else candidate_dir / "candidate.nnue2"
    manifest_path = Path(args.manifest) if args.manifest else candidate_dir / "CANDIDATE_MANIFEST.json"
    model_card_path = Path(args.model_card) if args.model_card else candidate_dir / "MODEL_CARD.json"
    checkpoint_path = Path(args.checkpoint) if args.checkpoint else candidate_dir / "checkpoint.pt"
    helper_bin = Path(args.format_helper) if args.format_helper else None

    errors: list[str] = []
    report = {
        "success": False,
        "candidate_path": str(candidate_path),
        "manifest_path": str(manifest_path),
        "model_card_path": str(model_card_path),
        "checkpoint_present": checkpoint_path.exists(),
        "format": "Unknown",
        "hash_checks_passed": False,
        "non_default_confirmed": False,
        "no_strength_claim_confirmed": False,
        "errors": errors,
    }

    for required in (candidate_path, manifest_path, model_card_path):
        if not required.exists():
            errors.append(f"missing required file: {required}")
    if errors:
        print(json.dumps(report, indent=2, sort_keys=True))
        if args.report:
            Path(args.report).write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        return 1

    manifest = _load_json(manifest_path)
    model_card = _load_json(model_card_path)

    if manifest.get("feature_set") != FEATURE_SET:
        errors.append("manifest feature_set mismatch")
    if manifest.get("features_per_perspective") != FEATURES_PER_PERSPECTIVE:
        errors.append("manifest features_per_perspective mismatch")
    if manifest.get("model_layout_name") != MODEL_LAYOUT_NAME:
        errors.append("manifest model_layout_name mismatch")
    if manifest.get("model_layout_version") != MODEL_LAYOUT_VERSION:
        errors.append("manifest model_layout_version mismatch")

    candidate_intent = str(manifest.get("candidate_intent", ""))
    model_status = str(model_card.get("status", ""))
    report["no_strength_claim_confirmed"] = _contains_no_strength_claim(candidate_intent) and _contains_no_strength_claim(model_status)
    if not report["no_strength_claim_confirmed"]:
        errors.append("missing no-strength/no-Elo claim statement")

    manifest_non_default = str(manifest.get("sirio_nnue2_default_status", ""))
    model_non_default = str(model_card.get("non_default_confirmation", ""))
    report["non_default_confirmed"] = _contains_non_default(manifest_non_default) and _contains_non_default(model_non_default)
    if not report["non_default_confirmed"]:
        errors.append("missing SirioNNUE2 non-default statement")

    model_training = model_card.get("training")
    if model_training != manifest.get("training"):
        errors.append("model-card training block mismatch with manifest")

    model_dataset = model_card.get("dataset")
    if model_dataset != manifest.get("dataset"):
        errors.append("model-card dataset block mismatch with manifest")

    try:
        artifact_block = manifest["artifacts"]
        expected_candidate_sha = artifact_block["candidate"]["sha256"]
        expected_checkpoint_sha = artifact_block["checkpoint"]["sha256"]
    except KeyError as exc:
        errors.append(f"manifest artifacts block missing key: {exc}")
        expected_candidate_sha = ""
        expected_checkpoint_sha = ""

    if expected_candidate_sha and _sha256(candidate_path) != expected_candidate_sha:
        errors.append("candidate.nnue2 sha256 mismatch")

    if checkpoint_path.exists():
        if expected_checkpoint_sha and _sha256(checkpoint_path) != expected_checkpoint_sha:
            errors.append("checkpoint.pt sha256 mismatch")
    else:
        errors.append("checkpoint.pt missing but required by manifest artifacts")

    report["hash_checks_passed"] = not any("sha256" in error for error in errors)

    try:
        fmt, diag = _detect_format_with_helper(candidate_path, helper_bin)
        report["format"] = fmt
        report["format_diagnostic"] = diag
        if fmt != "SirioNNUE2MinimalV1":
            export_mode = str(manifest.get("export", {}).get("mode", ""))
            if export_mode == "checkpoint" and _runtime_smoke_format_check(candidate_path):
                report["format"] = "SirioNNUE2MinimalV1"
                report["format_diagnostic"] = "runtime smoke contract accepted artifact"
            else:
                errors.append(f"candidate format mismatch: expected SirioNNUE2MinimalV1, got {fmt}")
    except Exception as exc:  # noqa: BLE001
        errors.append(str(exc))

    report["success"] = not errors
    output = json.dumps(report, indent=2, sort_keys=True)
    print(output)
    if args.report:
        Path(args.report).write_text(output + "\n", encoding="utf-8")
    return 0 if report["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
