#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE="$ROOT/build/sirioc"

if [[ ! -x "$ENGINE" ]]; then
    echo "Engine binary not found at $ENGINE" >&2
    exit 1
fi

printf "fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1\nmoves\nquit\n" | "$ENGINE" >/dev/null

