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

## UCI mode and engine options

SirioC also exposes a lightweight Universal Chess Interface (UCI) frontend for
GUI integrations and automated testing. Build the standalone C engine via
`make -C src` and launch it with the `--uci` switch:

```bash
cd src
make
./sirio --uci
```

When the engine receives the `uci` command it reports the following configurable
options:

| Option | Type | Description |
| --- | --- | --- |
| `UseNNUE` | `check` | Enables or disables the embedded HalfKP neural evaluator. Set to `false` to fall back to the material heuristic. |
| `EvalFile` | `string` | Path to the primary HalfKP network (`.nnue`). The engine falls back to material evaluation if the file cannot be loaded. |
| `EvalFileSmall` | `string` | Optional secondary network that is automatically selected for simplified positions (12 pieces or fewer). Provide an empty value to clear it. |

The engine expects Stockfish-compatible UCI flows and responds to `isready`,
`ucinewgame`, `position`, and `go` commands. A tiny regression bench executes
`uci`, `isready`, `ucinewgame`, `position startpos`, and `go depth 1`, checking
that a legal `bestmove` is returned. Invoke it with `make -C src bench`.

### NNUE weight files

SirioC does not ship binary NNUE weights. To use HalfKP evaluation, download a
compatible network (for example, from the [official Stockfish testing
server](https://tests.stockfishchess.org/nns/)) and point the `EvalFile` option
to its location. Stockfish-provided networks are distributed under the CC0
license, so they can be redistributed with SirioC-compatible builds.

A convenience script, `scripts/download_nnue.sh`, fetches the current
Stockfish main network (`sirio_default.nnue`) and the compact "small" network
(`sirio_small.nnue`) into `resources/`:

```bash
./scripts/download_nnue.sh
```

After the files are available, set `EvalFile`/`EvalFileSmall` accordingly.
When the small network is configured, SirioC automatically switches to it in
positions with 12 or fewer pieces to improve accuracy in simplified endgames.
If you prefer to bundle the default weights with release binaries, configure
CMake with `-DSIRIOC_EMBED_NNUE=ON` and ensure that
`resources/sirio_default.nnue` exists at configure time. When no NNUE file is
configured the engine falls back to its built-in material evaluator.

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
