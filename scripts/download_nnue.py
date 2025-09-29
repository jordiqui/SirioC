#!/usr/bin/env python3
"""Download the Stockfish NNUE networks used by SirioC."""

from __future__ import annotations

import argparse
import hashlib
import sys
import urllib.error
import urllib.request
from pathlib import Path
from typing import Dict, Iterable, Optional


NETWORKS: Dict[str, Dict[str, Optional[str]]] = {
    "nn-1c0000000000.nnue": {
        "url": "https://tests.stockfishchess.org/api/nn/nn-1c0000000000.nnue",
        "sha256": None,
    },
    "nn-37f18f62d772.nnue": {
        "url": "https://tests.stockfishchess.org/api/nn/nn-37f18f62d772.nnue",
        "sha256": None,
    },
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "networks",
        nargs="*",
        choices=sorted(NETWORKS.keys()),
        help="Subset of networks to download (defaults to all).",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path.cwd(),
        help="Directory where the NNUE files will be stored.",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Overwrite existing files instead of skipping them.",
    )
    return parser.parse_args()


def sha256sum(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as infile:
        for chunk in iter(lambda: infile.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def download(url: str, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    with urllib.request.urlopen(url) as response, destination.open("wb") as outfile:
        while True:
            chunk = response.read(1 << 20)
            if not chunk:
                break
            outfile.write(chunk)


def verify(path: Path, expected: Optional[str]) -> bool:
    if expected is None:
        return True
    actual = sha256sum(path)
    if actual.lower() != expected.lower():
        print(
            f"Checksum mismatch for {path.name}: expected {expected}, got {actual}.",
            file=sys.stderr,
        )
        return False
    return True


def run(networks: Iterable[str], output_dir: Path, force: bool) -> int:
    exit_code = 0
    for name in networks:
        info = NETWORKS[name]
        target = output_dir / name
        if target.exists() and not force:
            print(f"Skipping {name}: already exists at {target} (use --force to overwrite).")
            continue
        print(f"Downloading {name}...")
        try:
            download(info["url"], target)
        except (OSError, urllib.error.URLError) as exc:  # pragma: no cover
            print(f"Failed to download {name}: {exc}", file=sys.stderr)
            if target.exists():
                try:
                    target.unlink()
                except OSError:
                    pass
            exit_code = 1
            continue
        if not verify(target, info.get("sha256")):
            exit_code = 1
    return exit_code


def main() -> int:
    args = parse_args()
    selection = args.networks or sorted(NETWORKS.keys())
    return run(selection, args.output_dir.resolve(), args.force)


if __name__ == "__main__":
    sys.exit(main())

