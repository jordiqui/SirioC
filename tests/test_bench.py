#!/usr/bin/env python3
import re
import subprocess
import sys
from pathlib import Path

def main() -> int:
    if len(sys.argv) != 2:
        print("usage: test_bench.py <path-to-engine>", file=sys.stderr)
        return 1

    engine_path = Path(sys.argv[1])
    if not engine_path.exists():
        print(f"engine binary not found: {engine_path}", file=sys.stderr)
        return 1

    proc = subprocess.Popen(
        [str(engine_path)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )

    script = "\n".join([
        "uci",
        "isready",
        "bench depth 4 threads 1",
        "quit",
    ]) + "\n"

    out, err = proc.communicate(script, timeout=15)

    if proc.returncode not in (0, None):
        raise RuntimeError(f"engine exited with code {proc.returncode}: {err}")

    bench_line = None
    for line in out.splitlines():
        if line.startswith("bench depth"):
            bench_line = line.strip()
            break

    if bench_line is None:
        raise RuntimeError(f"bench command did not produce output. Output was:\n{out}")

    match = re.search(r"nps (\d+)", bench_line)
    if not match:
        raise RuntimeError(f"bench output missing nps value: {bench_line}")

    nps = int(match.group(1))
    if nps <= 0:
        raise RuntimeError(f"bench reported non-positive nps: {bench_line}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
