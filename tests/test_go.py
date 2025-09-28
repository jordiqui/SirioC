#!/usr/bin/env python3
import subprocess  # Needed to spawn the engine and helper processes
import sys
import textwrap


def run_engine(engine_path, commands):
    proc = subprocess.Popen(
        [engine_path],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    script = "\n".join(commands) + "\n"
    out, err = proc.communicate(script, timeout=5)
    if proc.returncode not in (0, None):
        raise RuntimeError(f"Engine exited with {proc.returncode}: {err}")
    bestmove = None
    for line in out.splitlines():
        if line.startswith("bestmove"):
            parts = line.split()
            if len(parts) >= 2:
                bestmove = parts[1]
            break
    if bestmove is None:
        raise AssertionError(f"Engine did not report bestmove. Output:\n{out}")
    return bestmove, out


def legal_moves(helper_path, fen):
    result = subprocess.run(
        [helper_path],
        input=fen,
        text=True,
        capture_output=True,
        check=True,
        timeout=5,
    )
    moves = result.stdout.strip().split()
    return moves


def assert_bestmove_is_legal(engine_path, helper_path, fen, position_cmd, expect_none=False):
    commands = [
        "uci",
        "isready",
        "ucinewgame",
        position_cmd,
        "go depth 1",
        "quit",
    ]
    bestmove, output = run_engine(engine_path, commands)
    moves = legal_moves(helper_path, fen)
    if expect_none:
        if moves:
            raise AssertionError(
                f"Expected no legal moves but helper produced: {moves}\nPosition: {position_cmd}"
            )
        if bestmove not in {"0000", "(none)"}:
            raise AssertionError(
                f"Engine should report 0000 (or historical (none)) but returned {bestmove}\nOutput:\n{output}"
            )
        return

    if not moves:
        raise AssertionError(f"Helper reported no legal moves for: {position_cmd}")
    if bestmove == "(none)":
        raise AssertionError(
            f"Engine reported no move for position expecting a legal move: {position_cmd}\n{output}"
        )
    assert bestmove in moves, textwrap.dedent(
        f"""
        Engine returned illegal move {bestmove}.
        Position command: {position_cmd}
        Legal moves: {moves}
        Engine output:\n{output}
        """
    )


def main():
    if len(sys.argv) != 3:
        print("Usage: test_go.py <engine> <helper>")
        return 2
    engine_path, helper_path = sys.argv[1:3]
    tests = [
        (
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            "position startpos",
            False,
        ),
        (
            "7k/8/8/8/8/8/6r1/7K w - - 0 1",
            "position fen 7k/8/8/8/8/8/6r1/7K w - - 0 1",
            False,
        ),
        (
            "7k/8/8/3pP3/8/8/8/7K w - d6 0 1",
            "position fen 7k/8/8/3pP3/8/8/8/7K w - d6 0 1",
            False,
        ),
        (
            "7k/5Q2/6K1/8/8/8/8/8 b - - 0 1",
            "position fen 7k/5Q2/6K1/8/8/8/8/8 b - - 0 1",
            True,
        ),
    ]
    for fen, cmd, expect_none in tests:
        assert_bestmove_is_legal(engine_path, helper_path, fen, cmd, expect_none)
    return 0


if __name__ == "__main__":
    sys.exit(main())
