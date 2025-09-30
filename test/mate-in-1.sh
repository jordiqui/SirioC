#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE="$ROOT/build/sirioc"

if [[ ! -x "$ENGINE" ]]; then
    echo "Engine binary not found at $ENGINE" >&2
    exit 1
fi

FEN="8/8/8/8/8/8/5K2/7k w - - 0 1"
"$ENGINE" --fen "${FEN// /_}" --evaluate --no-cli
"$ENGINE" --fen "${FEN// /_}" --print --no-cli

