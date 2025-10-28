"""Utilities to run automated regression matches for SirioC NNUE networks.

This module exposes a command-line interface that coordinates match runners
such as ``cutechess-cli`` or ``fastchess``.  Given a directory with validated
candidate networks, the orchestrator periodically pits them against a baseline
engine configuration and promotes the best performers to production.

The script purposely keeps the orchestration logic self-contained so it can be
used from CI pipelines, cron jobs or local experiments without additional
infrastructure.  It is tolerant to partially processed runs (results are
persisted to JSON files) and can resume after interruptions.
"""

from __future__ import annotations

import argparse
import dataclasses
import datetime as dt
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
from typing import Iterable, List, Optional


@dataclasses.dataclass
class MatchSummary:
    """Structured representation of a match outcome."""

    timestamp: str
    tool: str
    baseline: str
    candidate: str
    rounds: int
    wins_baseline: float
    wins_candidate: float
    draws: float
    points_baseline: float
    points_candidate: float
    raw_output_path: str
    promoted: bool

    def to_dict(self) -> dict:
        return dataclasses.asdict(self)


def parse_score(
    tool_output: str, baseline_name: str, candidate_name: str
) -> tuple[float, float, float, float, float]:
    """Extract match statistics from the orchestration output.

    The parser understands the most common summary formats produced by
    ``cutechess-cli`` and ``fastchess``.  The function falls back to a generic
    integer triplet (``<wins>-<losses>-<draws>``) search to remain compatible
    with custom ``-resultformat`` settings.
    """

    patterns = [
        re.compile(
            r"Score of (?P<baseline>.+?) vs (?P<candidate>.+?):\s+"
            r"(?P<wins>\d+) - (?P<losses>\d+) - (?P<draws>\d+)"
        ),
        re.compile(
            r"Wins=(?P<wins>\d+),\s*Draws=(?P<draws>\d+),\s*Losses=(?P<losses>\d+)"
        ),
        re.compile(
            r"Final score:\s*(?P<baseline>.+?)\s+"
            r"(?P<points_a>\d+(?:\.\d+)?)\s*-\s*"
            r"(?P<points_b>\d+(?:\.\d+)?)\s*(?P<candidate>.+)"
        ),
        re.compile(r"Result:?\s*(?P<wins>\d+)\s*-\s*(?P<losses>\d+)\s*-\s*(?P<draws>\d+)")
    ]

    for pattern in patterns:
        match = pattern.search(tool_output)
        if not match:
            continue

        groups = match.groupdict()

        if {"points_a", "points_b"}.issubset(groups):
            # Pattern with half-points (FastChess summary).
            points_baseline = float(groups["points_a"])
            points_candidate = float(groups["points_b"])
            return points_baseline, points_candidate, 0.0, points_baseline, points_candidate

        wins = float(groups["wins"])
        losses = float(groups["losses"])
        draws = float(groups.get("draws", 0.0))

        # Ensure the order matches the ``baseline -> candidate`` perspective.
        if groups.get("baseline") and groups.get("candidate"):
            baseline = groups["baseline"].strip()
            candidate = groups["candidate"].strip()
            if baseline_name and baseline_name not in baseline:
                # Engines were swapped; invert results.
                wins, losses = losses, wins

        points_baseline = wins + draws * 0.5
        points_candidate = losses + draws * 0.5

        return wins, losses, draws, points_baseline, points_candidate

    raise ValueError(
        "Could not parse match summary from the orchestration output. "
        "Ensure the runner prints standard score lines."
    )


def build_tool_command(
    *,
    tool: str,
    tool_path: Path,
    engine_path: Path,
    baseline_network: Path,
    candidate_network: Path,
    rounds: int,
    concurrency: int,
    opening: Optional[Path],
    moves: Optional[int],
    time_control: str,
    threads: int,
    adjudication: Optional[int],
    extra_args: Iterable[str],
) -> List[str]:
    """Build the subprocess invocation for the selected match tool."""

    baseline_name = "baseline"
    candidate_name = candidate_network.stem

    if tool == "cutechess":
        cmd: List[str] = [
            str(tool_path),
            "-repeat",
            "-rounds",
            str(rounds),
            "-tournament",
            "gauntlet",
        ]

        if concurrency:
            cmd += ["-concurrency", str(concurrency)]

        cmd += [
            "-engine",
            f"name={baseline_name}",
            f"cmd={engine_path}",
            f"option.EvalFile={baseline_network}",
            f"option.Threads={threads}",
        ]

        cmd += [
            "-engine",
            f"name={candidate_name}",
            f"cmd={engine_path}",
            f"option.EvalFile={candidate_network}",
            f"option.Threads={threads}",
        ]

        cmd += ["-games", str(rounds)]

        if time_control:
            cmd += ["-each", f"tc={time_control}", f"timemargin=0"]

        if opening:
            cmd += ["-openings", f"file={opening}", "order=random"]

        if moves:
            cmd += ["-draw", f"movenumber={moves}", "movecount=2", "score=5"]

        if adjudication:
            cmd += ["-resign", f"movecount={adjudication}", "score=400"]

    elif tool == "fastchess":
        cmd = [
            str(tool_path),
            "run",
            "--engine",
            f"name={baseline_name}",
            f"cmd={engine_path}",
            f"option.EvalFile={baseline_network}",
            f"option.Threads={threads}",
            "--engine",
            f"name={candidate_name}",
            f"cmd={engine_path}",
            f"option.EvalFile={candidate_network}",
            f"option.Threads={threads}",
            "--rounds",
            str(rounds),
            "--games",
            str(rounds),
            "--concurrency",
            str(concurrency or 1),
            "--time-control",
            time_control,
        ]

        if opening:
            cmd += ["--opening-file", str(opening)]

        if moves:
            cmd += ["--draw-moves", str(moves)]

        if adjudication:
            cmd += ["--adjudication-moves", str(adjudication)]

    else:
        raise ValueError(f"Unsupported orchestration tool '{tool}'.")

    cmd.extend(extra_args)
    return cmd


def promote_network(source: Path, destination: Path) -> None:
    """Deploy ``source`` into ``destination`` preserving metadata when possible."""

    destination.parent.mkdir(parents=True, exist_ok=True)

    if destination.exists() and destination.is_file():
        backup = destination.with_suffix(destination.suffix + ".bak")
        shutil.copy2(destination, backup)

    if destination.is_dir():
        shutil.copy2(source, destination / source.name)
    else:
        shutil.copy2(source, destination)


def run_match(
    *,
    tool: str,
    tool_path: Path,
    engine_path: Path,
    baseline_network: Path,
    candidate_network: Path,
    rounds: int,
    concurrency: int,
    opening: Optional[Path],
    moves: Optional[int],
    time_control: str,
    threads: int,
    adjudication: Optional[int],
    extra_args: Iterable[str],
    raw_output_path: Path,
) -> str:
    """Execute the match and persist the raw output."""

    command = build_tool_command(
        tool=tool,
        tool_path=tool_path,
        engine_path=engine_path,
        baseline_network=baseline_network,
        candidate_network=candidate_network,
        rounds=rounds,
        concurrency=concurrency,
        opening=opening,
        moves=moves,
        time_control=time_control,
        threads=threads,
        adjudication=adjudication,
        extra_args=extra_args,
    )

    process = subprocess.run(
        command,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
    )

    raw_output_path.write_text(process.stdout, encoding="utf-8")
    return process.stdout


def should_promote(points_baseline: float, points_candidate: float, threshold: float) -> bool:
    """Decide whether the candidate network deserves promotion."""

    margin = points_candidate - points_baseline
    return margin > threshold


def load_history(history_path: Path) -> List[dict]:
    if history_path.exists():
        return json.loads(history_path.read_text(encoding="utf-8"))
    return []


def save_history(history_path: Path, summaries: List[dict]) -> None:
    history_path.parent.mkdir(parents=True, exist_ok=True)
    history_path.write_text(json.dumps(summaries, indent=2), encoding="utf-8")


def collect_candidates(directory: Path, pattern: str) -> List[Path]:
    return sorted(directory.glob(pattern))


def parse_args(argv: Optional[List[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument("--tool", choices=["cutechess", "fastchess"], default="cutechess")
    parser.add_argument("--tool-path", type=Path, required=True, help="Path to cutechess-cli or fastchess executable")
    parser.add_argument("--engine", type=Path, required=True, help="Path to the SirioC engine binary")
    parser.add_argument("--baseline-network", type=Path, required=True, help="Reference NNUE network to defend the slot")
    parser.add_argument("--validated-dir", type=Path, required=True, help="Directory with candidate networks ready for regression")
    parser.add_argument("--deploy-path", type=Path, required=True, help="Destination path (file or directory) for promoted networks")
    parser.add_argument("--rounds", type=int, default=100, help="Number of match rounds to play per candidate")
    parser.add_argument("--concurrency", type=int, default=2, help="Concurrent games for the orchestrator")
    parser.add_argument("--time-control", type=str, default="40/5+0.1", help="Time control passed to the orchestrator")
    parser.add_argument("--threads", type=int, default=1, help="Threads per engine instance")
    parser.add_argument("--opening", type=Path, help="Opening book used for the matches")
    parser.add_argument("--moves", type=int, help="Number of moves before adjudicating a draw")
    parser.add_argument("--adjudication", type=int, help="Number of moves before adjudicating a resignation")
    parser.add_argument("--extra-args", nargs=argparse.REMAINDER, default=[], help="Additional arguments forwarded to the tool")
    parser.add_argument("--pattern", default="*.nnue", help="Glob used to discover candidate networks")
    parser.add_argument("--history", type=Path, default=Path("training/nnue/metrics/regressions/history.json"))
    parser.add_argument("--interval", type=int, default=0, help="Seconds to wait before rescanning candidates (0 runs once)")
    parser.add_argument("--promotion-threshold", type=float, default=0.5, help="Minimum point margin required for promotion")

    return parser.parse_args(argv)


def main(argv: Optional[List[str]] = None) -> int:
    args = parse_args(argv)

    history_path: Path = args.history
    results_dir = history_path.parent
    results_dir.mkdir(parents=True, exist_ok=True)

    history = load_history(history_path)
    tested_candidates = {entry["candidate"] for entry in history}

    while True:
        candidates = collect_candidates(args.validated_dir, args.pattern)
        new_candidates = [c for c in candidates if c.resolve().as_posix() not in tested_candidates]

        if not new_candidates and args.interval <= 0:
            print("No new candidate networks found. Exiting.")
            return 0

        for candidate in new_candidates:
            print(f"Evaluating candidate: {candidate}")
            timestamp = dt.datetime.utcnow().isoformat()
            raw_log = results_dir / f"{candidate.stem}_{timestamp}.log"

            try:
                output = run_match(
                    tool=args.tool,
                    tool_path=args.tool_path,
                    engine_path=args.engine,
                    baseline_network=args.baseline_network,
                    candidate_network=candidate,
                    rounds=args.rounds,
                    concurrency=args.concurrency,
                    opening=args.opening,
                    moves=args.moves,
                    time_control=args.time_control,
                    threads=args.threads,
                    adjudication=args.adjudication,
                    extra_args=args.extra_args,
                    raw_output_path=raw_log,
                )
            except subprocess.CalledProcessError as exc:
                print(f"Match runner failed for {candidate}: {exc}", file=sys.stderr)
                continue

            try:
                (
                    wins_baseline,
                    wins_candidate,
                    draws,
                    points_baseline,
                    points_candidate,
                ) = parse_score(output, "baseline", candidate.stem)
            except ValueError as exc:
                print(f"Could not parse match summary for {candidate}: {exc}", file=sys.stderr)
                continue

            promoted = should_promote(points_baseline, points_candidate, args.promotion_threshold)

            if promoted:
                promote_network(candidate, args.deploy_path)
                print(f"Candidate {candidate} promoted to {args.deploy_path}")
            else:
                print(f"Candidate {candidate} did not meet the promotion threshold.")

            summary = MatchSummary(
                timestamp=timestamp,
                tool=args.tool,
                baseline=str(args.baseline_network.resolve()),
                candidate=str(candidate.resolve()),
                rounds=args.rounds,
                wins_baseline=wins_baseline,
                wins_candidate=wins_candidate,
                draws=draws,
                points_baseline=points_baseline,
                points_candidate=points_candidate,
                raw_output_path=str(raw_log.resolve()),
                promoted=promoted,
            )

            history.append(summary.to_dict())
            tested_candidates.add(str(candidate.resolve()))
            save_history(history_path, history)

        if args.interval <= 0:
            break

        print(f"Sleeping for {args.interval} seconds before rescanning candidates...")
        time.sleep(args.interval)

    return 0


if __name__ == "__main__":  # pragma: no cover - CLI entry-point
    raise SystemExit(main())
