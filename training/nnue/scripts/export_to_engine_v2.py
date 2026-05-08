"""Export SirioNNUE2 network binaries in deterministic dummy or validated-checkpoint mode."""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path
from typing import Any

import torch

MAGIC = b"SirioNNUE2\0\0"
VERSION = 2
FEATURE_SET_ID = 1
PERSPECTIVE_COUNT = 2
FEATURES_PER_PERSPECTIVE = 40960
ACCUMULATOR_SIZE = 256
HIDDEN_DIMENSIONS = 256
OUTPUT_DIMENSIONS = 1
QUANT_INPUT_SCALE = 256
QUANT_OUTPUT_SCALE = 256

EXPECTED_FEATURE_SET = "SirioHalfKAv1"
EXPECTED_TRAINER_SCRIPT = "training.nnue.scripts.train_v2"
EXPECTED_EMBEDDING_SHAPE = (FEATURES_PER_PERSPECTIVE, ACCUMULATOR_SIZE)
EXPECTED_HEAD0_SHAPE = (HIDDEN_DIMENSIONS, ACCUMULATOR_SIZE)
EXPECTED_HEAD2_SHAPE = (OUTPUT_DIMENSIONS, HIDDEN_DIMENSIONS)


def _deterministic_i16(index: int) -> int:
    return ((index * 17 + 23) % 2047) - 1023


def build_dummy_payload() -> tuple[bytes, bytes, bytes, bytes]:
    input_count = FEATURES_PER_PERSPECTIVE * ACCUMULATOR_SIZE
    hidden_count = ACCUMULATOR_SIZE
    output_count = ACCUMULATOR_SIZE

    input_weights = struct.pack("<" + "h" * input_count, *(_deterministic_i16(i) for i in range(input_count)))
    hidden_bias = struct.pack("<" + "h" * hidden_count, *(_deterministic_i16(100000 + i) for i in range(hidden_count)))
    output_weights = struct.pack("<" + "h" * output_count, *(_deterministic_i16(200000 + i) for i in range(output_count)))
    output_bias = struct.pack("<i", 1337)
    return input_weights, hidden_bias, output_weights, output_bias


def build_header(sections: tuple[bytes, bytes, bytes, bytes]) -> bytes:
    iw, hb, ow, ob = sections
    payload_bytes = len(iw) + len(hb) + len(ow) + len(ob)
    checksum = 0
    return struct.pack(
        "<12sHHHIIIIIIIIIIIII",
        MAGIC,
        VERSION,
        FEATURE_SET_ID,
        0,
        FEATURES_PER_PERSPECTIVE,
        PERSPECTIVE_COUNT,
        ACCUMULATOR_SIZE,
        HIDDEN_DIMENSIONS,
        OUTPUT_DIMENSIONS,
        QUANT_INPUT_SCALE,
        QUANT_OUTPUT_SCALE,
        len(iw),
        len(hb),
        len(ow),
        len(ob),
        payload_bytes,
        checksum,
    )


def _validate_checkpoint_structure(payload: dict[str, Any], checkpoint_path: Path) -> None:
    if "metadata" not in payload:
        raise ValueError(f"checkpoint missing metadata: {checkpoint_path}")
    if "model_config" not in payload:
        raise ValueError(f"checkpoint missing model_config: {checkpoint_path}")
    if "state_dict" not in payload:
        raise ValueError(f"checkpoint missing state_dict: {checkpoint_path}")

    metadata = payload["metadata"]
    if not isinstance(metadata, dict):
        raise ValueError(f"checkpoint metadata must be an object: {checkpoint_path}")
    if metadata.get("feature_set") != EXPECTED_FEATURE_SET:
        raise ValueError(
            f"checkpoint feature_set mismatch: expected {EXPECTED_FEATURE_SET}, got {metadata.get('feature_set')!r}"
        )
    if int(metadata.get("features_per_perspective", -1)) != FEATURES_PER_PERSPECTIVE:
        raise ValueError(
            "checkpoint features_per_perspective mismatch: "
            f"expected {FEATURES_PER_PERSPECTIVE}, got {metadata.get('features_per_perspective')!r}"
        )
    if metadata.get("script_name") != EXPECTED_TRAINER_SCRIPT:
        raise ValueError(
            "checkpoint is not from train_v2 path: "
            f"expected script_name={EXPECTED_TRAINER_SCRIPT!r}, got {metadata.get('script_name')!r}"
        )


def _validate_checkpoint_architecture(payload: dict[str, Any], checkpoint_path: Path) -> None:
    model_config = payload["model_config"]
    if not isinstance(model_config, dict):
        raise ValueError(f"checkpoint model_config must be an object: {checkpoint_path}")
    for field in ("accumulator_size", "hidden_size"):
        if field not in model_config:
            raise ValueError(f"checkpoint model_config missing '{field}': {checkpoint_path}")

    state_dict = payload["state_dict"]
    if not isinstance(state_dict, dict):
        raise ValueError(f"checkpoint state_dict must be a mapping: {checkpoint_path}")
    required_keys = ("embedding.weight", "head.0.weight", "head.0.bias", "head.2.weight", "head.2.bias")
    for key in required_keys:
        if key not in state_dict:
            raise ValueError(f"checkpoint missing state_dict key '{key}': {checkpoint_path}")

    embedding_shape = tuple(state_dict["embedding.weight"].shape)
    head0_shape = tuple(state_dict["head.0.weight"].shape)
    head2_shape = tuple(state_dict["head.2.weight"].shape)

    incompatibilities: list[str] = []
    if embedding_shape != EXPECTED_EMBEDDING_SHAPE:
        incompatibilities.append(f"embedding.weight shape {embedding_shape} != {EXPECTED_EMBEDDING_SHAPE}")
    if head0_shape != EXPECTED_HEAD0_SHAPE:
        incompatibilities.append(f"head.0.weight shape {head0_shape} != {EXPECTED_HEAD0_SHAPE}")
    if head2_shape != EXPECTED_HEAD2_SHAPE:
        incompatibilities.append(f"head.2.weight shape {head2_shape} != {EXPECTED_HEAD2_SHAPE}")

    if incompatibilities:
        joined = "; ".join(incompatibilities)
        raise ValueError(
            "checkpoint architecture is incompatible with current SirioNNUE2 v2 binary payload layout; "
            f"mapping safely deferred ({joined})"
        )


def build_checkpoint_payload(checkpoint_path: str) -> tuple[bytes, bytes, bytes, bytes]:
    checkpoint = Path(checkpoint_path)
    payload = torch.load(checkpoint, map_location="cpu")
    if not isinstance(payload, dict):
        raise ValueError(f"checkpoint root must be a dict: {checkpoint}")
    _validate_checkpoint_structure(payload, checkpoint)
    _validate_checkpoint_architecture(payload, checkpoint)
    raise ValueError(
        "checkpoint export is currently rejected: train_v2 minimal model layout does not yet have a "
        "safe, documented one-to-one mapping to SirioNNUE2 binary payload sections"
    )


def export(output_path: str, checkpoint_path: str | None = None) -> dict[str, int | str]:
    sections = build_dummy_payload() if checkpoint_path is None else build_checkpoint_payload(checkpoint_path)
    header = build_header(sections)
    output = Path(output_path)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as handle:
        handle.write(header)
        for section in sections:
            handle.write(section)
    mode = "dummy" if checkpoint_path is None else "checkpoint"
    return {
        "mode": mode,
        "header_bytes": len(header),
        "payload_bytes": sum(len(s) for s in sections),
        "file_bytes": len(header) + sum(len(s) for s in sections),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, help="Output SirioNNUE2 binary path")
    parser.add_argument("--checkpoint", type=str, default=None, help="train_v2 checkpoint path for validated export")
    parser.add_argument("--describe", action="store_true", help="Print exporter metadata as JSON")
    args = parser.parse_args()

    if FEATURES_PER_PERSPECTIVE <= 0 or ACCUMULATOR_SIZE <= 0:
        raise ValueError("invalid dimensions for SirioNNUE2 export")

    stats = export(args.output, args.checkpoint)
    if args.describe:
        print(json.dumps(stats, indent=2))


if __name__ == "__main__":
    main()
