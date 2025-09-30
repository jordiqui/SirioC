# SirioC (pyrrhic edition)

This repository contains a fresh, fully documented rewrite of SirioC that adopts
Berserk's directory layout. The goal of the new code base is to offer a compact,
readable engine skeleton that can be extended with stronger search heuristics or
neural evaluation in the future. Instead of porting the historical sources, the
engine has been rebuilt from the ground up with modern C++ and a modular
architecture.

```
.
├── .github/           GitHub actions and automation hooks
├── resources/        Sample assets such as opening books or PGN collections
├── src/
│   ├── files/        PGN/FEN parsing utilities
│   ├── nn/           Lightweight evaluation helpers
│   └── pyrrhic/      Core engine logic and CLI entry point
└── test/             Self-contained regression checks
```

## Features

* **FEN support** – load and export Forsyth–Edwards Notation positions with
  sanity checking.
* **Pseudo-legal move generator** – generate sliding, knight, king, and pawn
  moves for exploratory analysis and heuristics.
* **Material evaluation** – a simple, configurable evaluator that scores boards
  by material balance, ideal for experimentation with additional terms.
* **PGN ingestion** – parse single-game PGN files to extract metadata and
  algebraic move sequences for training or analysis pipelines.
* **Interactive shell** – a minimal REPL that exposes FEN loading, evaluation,
  move listing, and PGN inspection commands.

## Building

The project uses CMake and requires a compiler with C++20 support.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

After a successful build the `sirioc` executable will be available in
`build/`. Launch it without arguments to start the interactive shell or pass
CLI flags to script common tasks:

```bash
./build/sirioc --help
./build/sirioc --fen rnbqkbnr_pppppppp_8_8_8_8_PPPPPPPP_RNBQKBNR_w_KQkq_-_0_1 --print
./build/sirioc --pgn resources/sample.pgn --evaluate --no-cli
```

Note that spaces inside FEN strings must be replaced with underscores when
passed via `--fen`. The interactive shell accepts regular space-delimited FEN
strings directly.

## Interactive commands

Run `sirioc` without arguments to enter the shell. Available commands:

```
help            Show the command list
fen <string>    Load a FEN position
show            Pretty-print the current board
moves           List pseudo-legal moves for the side to move
best            Suggest the highest-value capture based on material
eval            Evaluate the position with the material heuristic
load <file>     Load a PGN game from disk
game            Display PGN metadata and the first moves
quit            Exit the shell
```

## Tests

Smoke tests live in the `test/` directory and can be executed with CTest:

```bash
cmake -S . -B build -DSIRIOC_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

The suite ensures that FEN parsing, material evaluation, and PGN ingestion work
as expected.

## License

SirioC continues to be distributed under the terms of the [MIT License](LICENSE).
