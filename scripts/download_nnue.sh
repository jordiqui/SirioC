#!/usr/bin/env bash
set -euo pipefail

show_help() {
    cat <<'USAGE'
Usage: download_nnue.sh [options]

Download CC0-licensed NNUE networks that are compatible with SirioC's
HalfKP evaluator. By default the script retrieves both the primary and
secondary (small) networks published by the Stockfish project and stores
them under resources/.

Options:
  --dir <path>         Target directory for the downloaded networks (default: resources/)
  --force              Re-download files even if they already exist
  --primary-only       Download only the primary network
  --small-only         Download only the reduced "small" network
  --primary-url <url>  Override the URL used for the primary network
  --small-url <url>    Override the URL used for the small network
  -h, --help           Show this help message and exit

Environment variables:
  SIRIO_NNUE_PRIMARY_URL  Alternative source for the primary network
  SIRIO_NNUE_SMALL_URL    Alternative source for the small network
USAGE
}

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET_DIR="$ROOT_DIR/resources"
FORCE_DOWNLOAD=0
DOWNLOAD_PRIMARY=1
DOWNLOAD_SMALL=1
PRIMARY_URL="${SIRIO_NNUE_PRIMARY_URL:-https://tests.stockfishchess.org/api/nn/nn-62ef826d1a6d.nnue}"
SMALL_URL="${SIRIO_NNUE_SMALL_URL:-https://tests.stockfishchess.org/api/nn/nn-5af11540bbfe.nnue}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dir)
            if [[ $# -lt 2 ]]; then
                echo "error: --dir requires an argument" >&2
                exit 1
            fi
            shift
            dir_arg="$1"
            if [[ $dir_arg == ~* ]]; then
                dir_arg="${dir_arg/#\~/$HOME}"
            fi
            if [[ $dir_arg = /* ]]; then
                mkdir -p "$dir_arg"
                TARGET_DIR="$(cd "$dir_arg" && pwd)"
            else
                mkdir -p "$ROOT_DIR/$dir_arg"
                TARGET_DIR="$(cd "$ROOT_DIR/$dir_arg" && pwd)"
            fi
            ;;
        --force)
            FORCE_DOWNLOAD=1
            ;;
        --primary-only)
            DOWNLOAD_SMALL=0
            ;;
        --small-only)
            DOWNLOAD_PRIMARY=0
            ;;
        --primary-url)
            if [[ $# -lt 2 ]]; then
                echo "error: --primary-url requires an argument" >&2
                exit 1
            fi
            shift
            PRIMARY_URL="$1"
            ;;
        --small-url)
            if [[ $# -lt 2 ]]; then
                echo "error: --small-url requires an argument" >&2
                exit 1
            fi
            shift
            SMALL_URL="$1"
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            echo "error: unknown option: $1" >&2
            show_help
            exit 1
            ;;
    esac
    shift
done

if [[ $DOWNLOAD_PRIMARY -eq 0 && $DOWNLOAD_SMALL -eq 0 ]]; then
    echo "Nothing to do. Enable at least one download target." >&2
    exit 1
fi

mkdir -p "$TARGET_DIR"
PRIMARY_DEST="$TARGET_DIR/sirio_default.nnue"
SMALL_DEST="$TARGET_DIR/sirio_small.nnue"

cleanup() {
    [[ -n ${TMP_FILE:-} && -f $TMP_FILE ]] && rm -f "$TMP_FILE"
}
trap cleanup EXIT

download_file() {
    local url="$1"
    local destination="$2"
    local label="$3"

    if [[ -f "$destination" && $FORCE_DOWNLOAD -eq 0 ]]; then
        echo "Skipping existing $label -> $destination"
        return
    fi

    echo "Downloading $label from $url"
    TMP_FILE="${destination}.tmp"
    rm -f "$TMP_FILE"
    if ! curl -L --fail --progress-bar "$url" -o "$TMP_FILE"; then
        echo "Failed to download $label" >&2
        rm -f "$TMP_FILE"
        exit 1
    fi
    mv "$TMP_FILE" "$destination"
    echo "Saved $label to $destination"
}

if [[ $DOWNLOAD_PRIMARY -eq 1 ]]; then
    download_file "$PRIMARY_URL" "$PRIMARY_DEST" "primary network"
fi

if [[ $DOWNLOAD_SMALL -eq 1 ]]; then
    download_file "$SMALL_URL" "$SMALL_DEST" "small network"
fi

echo "Done. You can point EvalFile/EvalFileSmall to the downloaded weights."
