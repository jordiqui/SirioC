#!/usr/bin/env python3
"""Real-time KPI logger for SirioC."""

import argparse
import shlex
import subprocess
import sys
import time
from typing import List, Optional


class KPITracker:
    """Tracks depth, throughput and stability metrics from UCI info lines."""

    def __init__(self, stability_threshold: int = 30) -> None:
        self.start_time = time.perf_counter()
        self.last_move: Optional[str] = None
        self.last_score: Optional[int] = None
        self.stability = 0
        self.threshold = stability_threshold

    def process_info(self, line: str) -> None:
        tokens = line.split()
        data = {}
        i = 1  # skip the leading "info"
        while i < len(tokens):
            token = tokens[i]
            if token == "depth" and i + 1 < len(tokens):
                data["depth"] = _safe_int(tokens[i + 1])
                i += 2
            elif token == "seldepth" and i + 1 < len(tokens):
                data["seldepth"] = _safe_int(tokens[i + 1])
                i += 2
            elif token == "time" and i + 1 < len(tokens):
                data["time_ms"] = _safe_int(tokens[i + 1])
                i += 2
            elif token == "nodes" and i + 1 < len(tokens):
                data["nodes"] = _safe_int(tokens[i + 1])
                i += 2
            elif token == "nps" and i + 1 < len(tokens):
                data["nps"] = _safe_int(tokens[i + 1])
                i += 2
            elif token == "score" and i + 2 < len(tokens):
                score_type = tokens[i + 1]
                score_value = _safe_int(tokens[i + 2])
                data["score"] = (score_type, score_value)
                i += 3
            elif token == "pv" and i + 1 < len(tokens):
                data["pv"] = tokens[i + 1 :]
                break
            else:
                i += 1

        self._emit_update(data)

    def _score_value(self, score) -> Optional[int]:
        if score is None:
            return None
        score_type, score_value = score
        if score_value is None:
            return None
        if score_type == "cp":
            return score_value
        if score_type == "mate":
            # Map mate distances to a large centipawn scale so stability can be tracked.
            direction = 1 if score_value >= 0 else -1
            return direction * (100000 - abs(score_value))
        return None

    def _score_text(self, score) -> Optional[str]:
        if score is None:
            return None
        score_type, score_value = score
        if score_value is None:
            return None
        if score_type == "cp":
            return f"{score_value}cp"
        if score_type == "mate":
            return f"mate {score_value}"
        return None

    def _emit_update(self, data) -> None:
        elapsed = time.perf_counter() - self.start_time
        parts: List[str] = [f"[{elapsed:7.2f}s]"]
        depth = data.get("depth")
        if depth is not None:
            parts.append(f"d={depth}")
        seldepth = data.get("seldepth")
        if seldepth is not None:
            parts.append(f"sd={seldepth}")
        nps = data.get("nps")
        if nps is not None:
            parts.append(f"nps={_format_int(nps)}")
        nodes = data.get("nodes")
        if nodes is not None:
            parts.append(f"nodes={_format_int(nodes)}")
        best_move = None
        pv = data.get("pv")
        if pv:
            best_move = pv[0]
            parts.append(f"best={best_move}")
        score = data.get("score")
        score_text = self._score_text(score)
        if score_text:
            parts.append(f"score={score_text}")

        numeric_score = self._score_value(score)
        if best_move and numeric_score is not None:
            if (
                self.last_move == best_move
                and self.last_score is not None
                and abs(numeric_score - self.last_score) <= self.threshold
            ):
                self.stability += 1
            else:
                self.stability = 1
            self.last_move = best_move
            self.last_score = numeric_score
            parts.append(f"stability={self.stability}")

        time_ms = data.get("time_ms")
        if time_ms is not None:
            parts.append(f"time={time_ms}ms")

        print(" ".join(parts), flush=True)


def _safe_int(value: str) -> Optional[int]:
    try:
        return int(value)
    except ValueError:
        return None


def _format_int(value: Optional[int]) -> str:
    if value is None:
        return "0"
    return f"{value:,}"


def _handshake(proc: subprocess.Popen) -> None:
    _send(proc, "uci")
    _wait_for(proc, "uciok")
    _send(proc, "isready")
    _wait_for(proc, "readyok")


def _wait_for(proc: subprocess.Popen, token: str) -> None:
    while True:
        line = proc.stdout.readline()
        if not line:
            raise RuntimeError(f"Engine terminated while waiting for {token}")
        cleaned = line.strip()
        if cleaned == token:
            return


def _send(proc: subprocess.Popen, command: str) -> None:
    proc.stdin.write(command + "\n")
    proc.stdin.flush()


def _build_position_command(fen: Optional[str], moves: Optional[str]) -> str:
    if fen and fen.lower() != "startpos":
        base = f"position fen {fen}"
    else:
        base = "position startpos"
    if moves:
        base += f" moves {moves}"
    return base


def _graceful_shutdown(proc: subprocess.Popen) -> None:
    try:
        if proc.poll() is None:
            _send(proc, "quit")
    except Exception:
        pass
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()


def run() -> int:
    parser = argparse.ArgumentParser(description="Stream SirioC KPIs in real time.")
    parser.add_argument("--engine", default="./build/sirio", help="Path to the SirioC binary")
    parser.add_argument("--fen", default="startpos", help="FEN string to analyse")
    parser.add_argument("--moves", default="", help="Moves to append after the initial position")
    parser.add_argument("--movetime", type=int, default=0, help="Time in milliseconds for go movetime")
    parser.add_argument("--depth", type=int, default=0, help="Optional depth limit")
    parser.add_argument(
        "--stability-threshold",
        type=int,
        default=30,
        help="Centipawn delta allowed to increment stability",
    )
    args = parser.parse_args()

    engine_cmd = shlex.split(args.engine) if isinstance(args.engine, str) else list(args.engine)
    try:
        proc = subprocess.Popen(
            engine_cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
    except OSError as exc:
        print(f"Failed to launch engine: {exc}", file=sys.stderr)
        return 1

    assert proc.stdout is not None
    assert proc.stdin is not None

    tracker: Optional[KPITracker] = None
    try:
        _handshake(proc)
        position_cmd = _build_position_command(args.fen, args.moves.strip())
        _send(proc, position_cmd)
        tracker = KPITracker(stability_threshold=args.stability_threshold)

        go_args = ["go"]
        if args.movetime > 0:
            go_args.extend(["movetime", str(args.movetime)])
        if args.depth > 0:
            go_args.extend(["depth", str(args.depth)])
        if len(go_args) == 1:
            go_args.append("infinite")
        _send(proc, " ".join(go_args))

        while True:
            line = proc.stdout.readline()
            if not line:
                break
            cleaned = line.strip()
            if not cleaned:
                continue
            if cleaned.startswith("info "):
                if tracker:
                    tracker.process_info(cleaned)
            elif cleaned.startswith("bestmove"):
                print(cleaned, flush=True)
                break
        if proc.poll() is None:
            _graceful_shutdown(proc)
        return 0
    except KeyboardInterrupt:
        print("\nStopping analysis...", file=sys.stderr)
        try:
            _send(proc, "stop")
            while True:
                line = proc.stdout.readline()
                if not line:
                    break
                cleaned = line.strip()
                if cleaned.startswith("info ") and tracker:
                    tracker.process_info(cleaned)
                elif cleaned.startswith("bestmove"):
                    print(cleaned, flush=True)
                    break
        finally:
            _graceful_shutdown(proc)
        return 0
    except Exception as exc:  # pragma: no cover - defensive path
        print(f"Error while running KPI logger: {exc}", file=sys.stderr)
        _graceful_shutdown(proc)
        return 1


if __name__ == "__main__":
    sys.exit(run())

