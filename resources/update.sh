#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TABLEBASE_DIR="$ROOT_DIR/resources/tablebases"
TABLEBASE_SOURCE="$ROOT_DIR/src/files/bench.csv"
TABLEBASE_TARGET="$TABLEBASE_DIR/basic.csv"
NETWORK_TARGET="$ROOT_DIR/resources/network.dat"

mkdir -p "$TABLEBASE_DIR"
cp "$TABLEBASE_SOURCE" "$TABLEBASE_TARGET"

echo "Copied miniature tablebase to $TABLEBASE_TARGET"

if [[ ! -f "$NETWORK_TARGET" ]]; then
    cat <<'MODEL' > "$NETWORK_TARGET"
# Default SirioC evaluation network
pawn=100
knight=325
bishop=330
rook=500
queen=900
king=20000
bias=0
MODEL
    echo "Wrote default evaluation weights to $NETWORK_TARGET"
else
    echo "Existing evaluation weights preserved at $NETWORK_TARGET"
fi

