#!/usr/bin/env python3
"""Utility to reproduce CCRL-style automatic matches for SirioC.

This script wraps ``cutechess-cli`` invocations in order to simulate the
most common time controls used by the Chess Engines Grand Tournament (CEGT)
and the Computer Chess Rating Lists (CCRL).  The objective is to have a
repeatable way to exercise the engine locally before submitting binaries
for official testing.

The tool expects that ``cutechess-cli`` is available in the ``PATH``.
The engine under test can be provided explicitly or autodetected from the
build tree.
"""
from __future__ import annotations

import argparse
import json
import shlex
import subprocess
import sys
import textwrap
import threading
import time
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Iterable, List, Optional

try:  # pragma: no cover - optional dependency
    import psutil  # type: ignore
except ImportError:  # pragma: no cover - optional dependency
    psutil = None  # type: ignore


@dataclass
class CCRLMatchConfig:
    """Metadata describing a CCRL-like benchmark."""

    name: str
    description: str
    tc: str
    rounds: int
    openings: Optional[Path]
    pgn_output: Optional[Path]

    def cutechess_args(self, engine: Path, opponent: Optional[Path]) -> List[str]:
        opponent_args: List[str]
        if opponent:
            opponent_args = [
                "-engine",
                f"name={engine.name}",
                f"cmd={engine}",
                "-engine",
                f"name={opponent.name}",
                f"cmd={opponent}",
            ]
        else:
            opponent_args = [
                "-engine",
                f"name={engine.name}",
                f"cmd={engine}",
                "-engine",
                f"name={engine.name}-clone",
                f"cmd={engine}",
                "proto=uci",
                "option.Threads=1",
            ]

        args = [
            *opponent_args,
            "-games",
            str(self.rounds),
            "-repeat",
            "-concurrency",
            "1",
            "-resign",
            "movecount=3",
            "score=600",
            "-draw",
            "movenumber=60",
            "movecount=6",
            "score=20",
            "-tournament",
            "gauntlet",
            "-each",
            f"tc={self.tc}",
            "proto=uci",
            "option.Threads=1",
        ]

        if self.openings:
            args.extend(["-openings", f"file={self.openings}", "format=pgn", "order=random"])
        if self.pgn_output:
            args.extend(["-pgnout", str(self.pgn_output)])
        return args


DEFAULT_MATCHES = (
    CCRLMatchConfig(
        name="ccrl_blitz",
        description="CCRL 40/4 Blitz configuration (2+0.1 when expressed as tc).",
        tc="40/4+0.1",
        rounds=20,
        openings=None,
        pgn_output=None,
    ),
    CCRLMatchConfig(
        name="ccrl_rapid",
        description="CCRL 40/15 Rapid configuration (15+0.1).",
        tc="40/15+0.1",
        rounds=8,
        openings=None,
        pgn_output=None,
    ),
    CCRLMatchConfig(
        name="ccrl_classic",
        description="Longer format close to CCRL 40/120 (fixed openings recommended).",
        tc="40/120+0.2",
        rounds=4,
        openings=None,
        pgn_output=None,
    ),
)


def detect_engine(build_dir: Path) -> Path:
    candidate = build_dir / "bin" / "sirio"
    if not candidate.exists():
        raise FileNotFoundError(
            f"No se encontró el binario en {candidate}. Compila el proyecto antes de ejecutar la suite."
        )
    return candidate


def log_resource_usage(proc: subprocess.Popen[bytes], log_path: Path, interval: float) -> None:
    if psutil is None:
        log_path.write_text(
            "psutil no está instalado. Ejecuta 'pip install psutil' para recopilar métricas de CPU y memoria.\n"
        )
        return

    try:
        process = psutil.Process(proc.pid)
    except psutil.Error as exc:  # pragma: no cover - depende del sistema
        log_path.write_text(
            f"No se pudieron obtener métricas de recursos: {exc}. Asegúrate de tener permisos suficientes.\n",
            encoding="utf-8",
        )
        return
    cpu_samples: List[float] = []
    rss_samples: List[int] = []
    with log_path.open("w", encoding="utf-8") as handler:
        handler.write("timestamp,cpu_percent,rss_bytes\n")
        process.cpu_percent(interval=None)  # primer muestreo para inicializar
        while proc.poll() is None:
            try:
                cpu = process.cpu_percent(interval=interval)
                mem = process.memory_info().rss
            except psutil.Error as exc:  # pragma: no cover - depende del sistema
                handler.write(f"0.000,0.0,0 # psutil error: {exc}\n")
                break
            timestamp = time.time()
            cpu_samples.append(cpu)
            rss_samples.append(mem)
            handler.write(f"{timestamp:.3f},{cpu:.3f},{mem}\n")
            handler.flush()

    summary = {
        "avg_cpu_percent": sum(cpu_samples) / len(cpu_samples) if cpu_samples else 0.0,
        "max_cpu_percent": max(cpu_samples) if cpu_samples else 0.0,
        "max_rss_bytes": max(rss_samples) if rss_samples else 0,
        "samples": len(cpu_samples),
        "interval_seconds": interval,
    }
    log_path.with_suffix(".json").write_text(json.dumps(summary, indent=2), encoding="utf-8")


def run_match(cfg: CCRLMatchConfig, args: argparse.Namespace) -> None:
    cutechess = Path(args.cutechess)
    if not cutechess.name:
        raise SystemExit("cutechess-cli no especificado")

    log_dir = Path(args.output)
    log_dir.mkdir(parents=True, exist_ok=True)

    match_cfg = replace(cfg)
    match_cfg.pgn_output = log_dir / f"{cfg.name}.pgn" if args.save_pgns else None
    match_cfg.openings = Path(args.openings) if args.openings else cfg.openings

    engine_path = Path(args.engine)
    opponent_path = Path(args.opponent) if args.opponent else None

    cmd = [str(cutechess), *match_cfg.cutechess_args(engine_path, opponent_path)]
    print(f"Ejecutando {cfg.name}:\n  {' '.join(shlex.quote(part) for part in cmd)}")

    with subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT) as proc:
        log_file = log_dir / f"{cfg.name}.log"
        monitor_log = log_dir / f"{cfg.name}_resources.csv"
        monitor_interval = args.monitor_interval
        monitor_thread: Optional[threading.Thread] = None
        if monitor_interval > 0:
            monitor_thread = threading.Thread(
                target=log_resource_usage, args=(proc, monitor_log, monitor_interval), daemon=True
            )
            monitor_thread.start()
        with log_file.open("wb") as handler:
            assert proc.stdout is not None
            for line in proc.stdout:
                handler.write(line)
                sys.stdout.buffer.write(line)
        return_code = proc.wait()
        if monitor_thread is not None:
            monitor_thread.join()
    if return_code != 0:
        raise SystemExit(f"La ejecución de cutechess-cli finalizó con código {return_code}")


def parse_args(argv: Optional[Iterable[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Reproduce partidas automáticas similares a CCRL usando cutechess-cli.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=textwrap.dedent(
            """
            Ejemplos:
              python bench/ccrl_suite.py --engine build/bin/sirio --cutechess cutechess-cli
              python bench/ccrl_suite.py --engine build/bin/sirio --cutechess cutechess-cli \\
                     --opponent /ruta/a/stockfish --openings bench/ccrl_openings.pgn
            """
        ),
    )
    parser.add_argument(
        "--engine",
        type=str,
        help="Ruta al ejecutable de SirioC (por defecto build/bin/sirio).",
    )
    parser.add_argument("--cutechess", type=str, default="cutechess-cli",
                        help="Comando a usar para cutechess-cli (debe estar en el PATH).")
    parser.add_argument("--opponent", type=str,
                        help="Motor rival opcional; si no se indica se realiza un auto-match.")
    parser.add_argument("--openings", type=str,
                        help="Libro PGN para sortear aperturas fijas (opcional).")
    parser.add_argument("--output", type=str, default="bench/results",
                        help="Directorio donde guardar logs, PGN y métricas.")
    parser.add_argument("--monitor-interval", dest="monitor_interval", type=float, default=1.0,
                        help="Intervalo en segundos para muestrear CPU/memoria (0 = desactivar).")
    parser.add_argument("--save-pgns", action="store_true",
                        help="Guardar los PGN generados por cada match.")
    parser.add_argument("--only", type=str, nargs="*",
                        help="Lista de benchmarks a ejecutar (por defecto todos).")
    return parser.parse_args(argv)


def main(argv: Optional[Iterable[str]] = None) -> None:
    args = parse_args(argv)

    if args.engine is None:
        args.engine = str(detect_engine(Path("build")))

    matches = {cfg.name: cfg for cfg in DEFAULT_MATCHES}
    selected: Iterable[CCRLMatchConfig] = list(matches.values())
    if args.only:
        missing = set(args.only) - set(matches.keys())
        if missing:
            raise SystemExit(f"Benchmarks desconocidos: {', '.join(sorted(missing))}")
        selected = [matches[name] for name in args.only]

    for cfg in selected:
        run_match(cfg, args)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        raise SystemExit(130)
