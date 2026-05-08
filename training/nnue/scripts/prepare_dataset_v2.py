"""Prepare SirioNNUE2 dataset-v2 sparse feature records from FEN-based inputs."""
from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import random
from collections import Counter
from dataclasses import dataclass

from .features_v2 import (
    BLACK,
    FEATURES_PER_PERSPECTIVE,
    WHITE,
    encode_sirio_halfka_v1,
)


@dataclass(frozen=True)
class ParsedRecord:
    fen: str
    score_cp: int
    result: str
    source: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, help="Input dataset path")
    parser.add_argument("--output-dir", required=True, help="Output directory for dataset-v2 splits")
    parser.add_argument("--format", choices=("auto", "tsv", "jsonl"), default="auto", help="Input format")
    parser.add_argument("--seed", type=int, default=20240511, help="Deterministic split seed")
    parser.add_argument("--val-ratio", type=float, default=0.1, help="Validation split ratio")
    parser.add_argument("--test-ratio", type=float, default=0.1, help="Test split ratio")
    parser.add_argument("--source", default="unknown", help="Default source label")
    parser.add_argument("--max-records", type=int, default=None, help="Optional maximum accepted records")
    return parser.parse_args()


def _ensure_fen_six_fields(fen: str) -> None:
    fields = fen.strip().split()
    if len(fields) != 6:
        raise ValueError(f"invalid FEN field count: expected 6, got {len(fields)}")


def _parse_tsv_line(line: str, default_source: str) -> ParsedRecord:
    stripped = line.strip()
    if not stripped:
        raise ValueError("empty line")
    tab_parts = stripped.split("\t")
    if len(tab_parts) >= 3:
        fen = tab_parts[0].strip()
        _ensure_fen_six_fields(fen)
        score_cp = int(tab_parts[1])
        result = tab_parts[2].strip()
        source = tab_parts[3].strip() if len(tab_parts) > 3 and tab_parts[3].strip() else default_source
        return ParsedRecord(fen=fen, score_cp=score_cp, result=result, source=source)

    parts = stripped.split()
    if len(parts) < 8:
        raise ValueError("tsv/whitespace record must contain six FEN fields + score_cp + result")
    fen = " ".join(parts[:6])
    _ensure_fen_six_fields(fen)
    score_cp = int(parts[6])
    result = parts[7]
    source = parts[8] if len(parts) > 8 else default_source
    return ParsedRecord(fen=fen, score_cp=score_cp, result=result, source=source)


def _parse_jsonl_line(line: str, default_source: str) -> ParsedRecord:
    payload = json.loads(line)
    fen = str(payload["fen"])
    _ensure_fen_six_fields(fen)
    score_cp = int(payload["score_cp"])
    result = str(payload.get("result", "*"))
    source = str(payload.get("source", default_source))
    return ParsedRecord(fen=fen, score_cp=score_cp, result=result, source=source)


def _wdl_from_result(result: str) -> float | None:
    mapping = {"1-0": 1.0, "1/2-1/2": 0.5, "0-1": 0.0, "*": None}
    if result not in mapping:
        raise ValueError(f"unsupported result value '{result}'")
    return mapping[result]


def _phase_bucket(fen: str) -> str:
    piece_values = {"P": 1, "N": 3, "B": 3, "R": 5, "Q": 9}
    material = 0
    for token in fen.split()[0]:
        if token.upper() in piece_values:
            material += piece_values[token.upper()]
    if material >= 40:
        return "opening"
    if material >= 15:
        return "middlegame"
    return "endgame"


def _convert_features(fen: str) -> dict[str, list[list[int]]]:
    sparse = encode_sirio_halfka_v1(fen)
    converted: dict[str, list[list[int]]] = {"white": [], "black": []}
    for label, perspective in (("white", WHITE), ("black", BLACK)):
        pairs = [[feature.index, feature.value] for feature in sparse[perspective]]
        for idx, value in pairs:
            if idx < 0 or idx >= FEATURES_PER_PERSPECTIVE:
                raise ValueError(f"feature index out of range: {idx}")
            if value != 1:
                raise ValueError(f"feature value must be 1, got {value}")
        converted[label] = pairs
    return converted


def _stable_key(fen: str, seed: int) -> str:
    return hashlib.sha256(f"{seed}|{fen}".encode("utf-8")).hexdigest()


def build_dataset(args: argparse.Namespace) -> None:
    if args.val_ratio < 0.0 or args.test_ratio < 0.0 or args.val_ratio + args.test_ratio >= 1.0:
        raise ValueError("val/test ratios must be >=0 and sum to <1")

    output_dir = pathlib.Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    accepted: list[dict[str, object]] = []
    rejected = 0
    rejection_reasons: Counter[str] = Counter()
    processed = 0

    with open(pathlib.Path(args.input), "r", encoding="utf-8") as handle:
        for raw in handle:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if args.max_records is not None and len(accepted) >= args.max_records:
                break
            processed += 1
            try:
                use_fmt = args.format
                if args.format == "auto":
                    use_fmt = "jsonl" if line.startswith("{") else "tsv"
                rec = _parse_jsonl_line(line, args.source) if use_fmt == "jsonl" else _parse_tsv_line(line, args.source)
                features = _convert_features(rec.fen)
                wdl = _wdl_from_result(rec.result)
                accepted.append({
                "fen": rec.fen,
                "features": features,
                "score_cp": rec.score_cp,
                "result": rec.result,
                "wdl": wdl,
                "phase": _phase_bucket(rec.fen),
                "source": rec.source,
                "feature_set": "SirioHalfKAv1",
                })
            except (ValueError, KeyError, json.JSONDecodeError) as exc:
                rejected += 1
                rejection_reasons[str(exc)] += 1

    ordering = sorted(range(len(accepted)), key=lambda i: (_stable_key(str(accepted[i]["fen"]), args.seed), i))
    ordered = [accepted[i] for i in ordering]

    total = len(ordered)
    test_count = int(total * args.test_ratio)
    val_count = int(total * args.val_ratio)
    train_count = total - val_count - test_count

    splits = {
        "train": ordered[:train_count],
        "val": ordered[train_count : train_count + val_count],
        "test": ordered[train_count + val_count :],
    }

    for name, rows in splits.items():
        with open(output_dir / f"{name}.jsonl", "w", encoding="utf-8") as handle:
            for row in rows:
                handle.write(json.dumps(row, sort_keys=True) + "\n")

    manifest = {
        "version": 2,
        "input_path": str(pathlib.Path(args.input).resolve()),
        "format": args.format,
        "feature_set": "SirioHalfKAv1",
        "score_cp_perspective": "white",
        "wdl_mapping": {"1-0": 1.0, "1/2-1/2": 0.5, "0-1": 0.0, "*": None},
        "phase_rule": "non-king material buckets: opening>=40, middlegame>=15, else endgame",
        "seed": args.seed,
        "val_ratio": args.val_ratio,
        "test_ratio": args.test_ratio,
        "processed_records": processed,
        "accepted_records": total,
        "rejected_records": rejected,
        "rejection_summary": dict(rejection_reasons),
        "split_counts": {k: len(v) for k, v in splits.items()},
        "source_default": args.source,
    }
    with open(output_dir / "MANIFEST.json", "w", encoding="utf-8") as handle:
        json.dump(manifest, handle, indent=2, sort_keys=True)


def main() -> None:
    build_dataset(parse_args())


if __name__ == "__main__":
    main()
