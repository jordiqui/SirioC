"""Export a deterministic dummy SirioNNUE2 network binary."""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path

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


def _deterministic_i16(index: int) -> int:
    return ((index * 17 + 23) % 2047) - 1023


def build_payload() -> tuple[bytes, bytes, bytes, bytes]:
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


def export(output_path: str) -> dict[str, int]:
    sections = build_payload()
    header = build_header(sections)
    output = Path(output_path)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as handle:
        handle.write(header)
        for section in sections:
            handle.write(section)
    return {
        "header_bytes": len(header),
        "payload_bytes": sum(len(s) for s in sections),
        "file_bytes": len(header) + sum(len(s) for s in sections),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, help="Output SirioNNUE2 binary path")
    parser.add_argument("--describe", action="store_true", help="Print exporter metadata as JSON")
    args = parser.parse_args()

    if FEATURES_PER_PERSPECTIVE <= 0 or ACCUMULATOR_SIZE <= 0:
        raise ValueError("invalid dimensions for SirioNNUE2 export")

    stats = export(args.output)
    if args.describe:
        print(json.dumps(stats, indent=2))


if __name__ == "__main__":
    main()
