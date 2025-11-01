#!/usr/bin/env python3
"""Run SirioC search benchmarks compatible with Stockfish/Berserk/Obsidian suites."""
from __future__ import annotations

import argparse
import json
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional

SUITE_FILES: Dict[str, str] = {
    "stockfish": "stockfish_suite.json",
    "berserk": "berserk_suite.json",
    "obsidian": "obsidian_suite.json",
}


@dataclass
class SuiteLimit:
    type: str
    value: int

    def to_go_command(self) -> str:
        if self.type == "depth":
            return f"go depth {self.value}"
        if self.type == "nodes":
            return f"go nodes {self.value}"
        if self.type == "movetime":
            return f"go movetime {self.value}"
        raise ValueError(f"Unsupported limit type: {self.type}")


@dataclass
class SuitePosition:
    fen: str
    moves: List[str]


@dataclass
class SuiteDefinition:
    name: str
    source: str
    limit: SuiteLimit
    threads: int
    positions: List[SuitePosition]

    @staticmethod
    def from_path(path: Path) -> "SuiteDefinition":
        data = json.loads(path.read_text())
        positions = [SuitePosition(entry["fen"], entry.get("moves", [])) for entry in data["positions"]]
        limit_data = data.get("limit", {"type": "depth", "value": 10})
        limit = SuiteLimit(limit_data["type"], int(limit_data["value"]))
        return SuiteDefinition(
            name=data.get("name", path.stem),
            source=data.get("source", "unknown"),
            limit=limit,
            threads=int(data.get("threads", 1)),
            positions=positions,
        )


@dataclass
class PositionResult:
    fen: str
    moves: List[str]
    bestmove: Optional[str]
    info_line: Optional[str]
    telemetry: Dict[str, object]
    elapsed_s: float


@dataclass
class SuiteResult:
    suite: SuiteDefinition
    results: List[PositionResult]
    total_nodes: int
    total_time_s: float

    def to_dict(self) -> Dict[str, object]:
        return {
            "suite": {
                "name": self.suite.name,
                "source": self.suite.source,
                "limit": {
                    "type": self.suite.limit.type,
                    "value": self.suite.limit.value,
                },
                "threads": self.suite.threads,
            },
            "positions": [
                {
                    "fen": result.fen,
                    "moves": result.moves,
                    "bestmove": result.bestmove,
                    "info": result.info_line,
                    "telemetry": result.telemetry,
                    "elapsed_s": result.elapsed_s,
                }
                for result in self.results
            ],
            "total_nodes": self.total_nodes,
            "total_time_s": self.total_time_s,
            "nodes_per_second": (
                self.total_nodes / self.total_time_s if self.total_time_s > 0 else 0.0
            ),
        }


class UCIEngine:
    def __init__(self, path: Path) -> None:
        self.process = subprocess.Popen(
            [str(path)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        self.send("uci")
        self._await_keyword("uciok", init=True)

    def close(self) -> None:
        if self.process.poll() is None:
            self.send("quit")
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.kill()

    def send(self, command: str) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()

    def _await_keyword(self, keyword: str, init: bool = False) -> None:
        assert self.process.stdout is not None
        while True:
            line = self.process.stdout.readline()
            if not line:
                raise RuntimeError(f"Engine terminated while waiting for {keyword}")
            line = line.strip()
            if init:
                # Skip id lines during initialization
                if line.startswith("id "):
                    continue
            if line == keyword:
                break

    def is_ready(self) -> None:
        self.send("isready")
        self._await_keyword("readyok")

    def set_option(self, name: str, value: object) -> None:
        self.send(f"setoption name {name} value {value}")

    def run_position(self, limit: SuiteLimit, position: SuitePosition) -> PositionResult:
        self.send("ucinewgame")
        parts = ["position", "fen", position.fen]
        if position.moves:
            parts.append("moves")
            parts.extend(position.moves)
        self.send(" ".join(parts))
        self.is_ready()
        go_command = limit.to_go_command()
        self.send(go_command)
        assert self.process.stdout is not None
        info_line: Optional[str] = None
        telemetry: Dict[str, object] = {}
        bestmove: Optional[str] = None
        start = time.perf_counter()
        while True:
            raw = self.process.stdout.readline()
            if not raw:
                raise RuntimeError("Engine terminated during search")
            line = raw.strip()
            if not line:
                continue
            if line.startswith("info string telemetry "):
                payload = line[len("info string telemetry ") :]
                try:
                    telemetry = json.loads(payload)
                except json.JSONDecodeError as exc:
                    raise RuntimeError(f"Failed to parse telemetry: {payload}") from exc
                continue
            if line.startswith("info "):
                info_line = line
                continue
            if line.startswith("bestmove "):
                bestmove = line.split()[1]
                break
        elapsed = time.perf_counter() - start
        nodes = int(telemetry.get("main_nodes", 0)) + int(telemetry.get("quiescence_nodes", 0))
        return PositionResult(
            fen=position.fen,
            moves=position.moves,
            bestmove=bestmove,
            info_line=info_line,
            telemetry={**telemetry, "nodes_total": nodes},
            elapsed_s=elapsed,
        )


def load_suites(names: Iterable[str]) -> List[SuiteDefinition]:
    suites = []
    base = Path(__file__).resolve().parent
    for name in names:
        key = name.lower()
        if key not in SUITE_FILES:
            raise ValueError(f"Unknown suite '{name}'. Available: {', '.join(sorted(SUITE_FILES))}")
        path = base / SUITE_FILES[key]
        suites.append(SuiteDefinition.from_path(path))
    return suites


def run_suite(
    engine: UCIEngine,
    suite: SuiteDefinition,
    threads_override: Optional[int],
    limit_override: Optional[SuiteLimit],
    position_cap: Optional[int],
) -> SuiteResult:
    threads = threads_override if threads_override is not None else suite.threads
    engine.set_option("Threads", threads)
    engine.is_ready()
    results: List[PositionResult] = []
    total_nodes = 0
    total_time = 0.0
    active_limit = limit_override if limit_override is not None else suite.limit
    for index, position in enumerate(suite.positions, start=1):
        if position_cap is not None and index > position_cap:
            break
        result = engine.run_position(active_limit, position)
        results.append(result)
        telemetry = result.telemetry
        nodes = int(telemetry.get("nodes_total", 0))
        total_nodes += nodes
        total_time += result.elapsed_s
        print(
            f"[{suite.name}] {index}/{len(suite.positions)} bestmove={result.bestmove} nodes={nodes}",
            flush=True,
        )
    return SuiteResult(suite=suite, results=results, total_nodes=total_nodes, total_time_s=total_time)


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, default=Path("./sirio"), help="Path to the engine binary")
    parser.add_argument(
        "--suite",
        action="append",
        dest="suites",
        help="Benchmark suite to run (stockfish, berserk, obsidian). Can be repeated.",
    )
    parser.add_argument("--threads", type=int, default=None, help="Override thread count")
    parser.add_argument("--output", type=Path, default=None, help="Optional path to write JSON results")
    parser.add_argument("--limit-type", choices=["depth", "nodes", "movetime"], help="Override limit type")
    parser.add_argument("--limit-value", type=int, help="Override limit value")
    parser.add_argument("--positions", type=int, default=None, help="Limit number of positions per suite")
    args = parser.parse_args(argv)

    suite_names = args.suites if args.suites else list(SUITE_FILES.keys())
    suites = load_suites(suite_names)

    engine = UCIEngine(args.engine)
    try:
        all_results = []
        for suite in suites:
            limit_override = None
            if args.limit_type and args.limit_value is not None:
                limit_override = SuiteLimit(args.limit_type, args.limit_value)
            result = run_suite(engine, suite, args.threads, limit_override, args.positions)
            print(
                f"Completed {suite.name}: nodes={result.total_nodes} time={result.total_time_s:.2f}s "
                f"nps={result.total_nodes / result.total_time_s if result.total_time_s > 0 else 0:.0f}"
            )
            all_results.append(result.to_dict())
        if args.output:
            output_data = {
                "generated_at": time.strftime("%Y-%m-%dT%H:%M:%S"),
                "engine": str(args.engine),
                "results": all_results,
            }
            args.output.write_text(json.dumps(output_data, indent=2))
    finally:
        engine.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
