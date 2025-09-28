# SirioC

SirioC is a modular, buildable UCI chess engine skeleton with NNUE scaffolding
and a focus on competitive default play. The current search integrates
heuristics inspired by the open-source Stockfish and Berserk projects, giving
the engine a strong baseline while leaving room for experimentation with new
ideas.

## Highlights

* Multi-threaded iterative deepening with late move reductions, killer/history
  heuristics, tapered evaluation, and Syzygy tablebase probing support.
* Optional NNUE evaluation infrastructure wired into the board representation
  so neural networks can be attached without reworking the move generator.
* Bench command and instrumentation hooks that make it easy to validate search
  changes or gather tuning data.

## Build

### Windows (MSYS2 MinGW64 or Clang)
```bash
mkdir build && cd build
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
```

If you know the target machines support AVX2 and BMI2, you can enable the
optimized build with `-DENABLE_AVX2=ON` when configuring CMake. Additional
flags are available to squeeze more performance out of modern CPUs:

* `-DENABLE_NATIVE=ON` adds `-march=native` when supported, letting the compiler
  use every instruction available on the build host.
* `-DENABLE_VNNI=ON` enables AVX-512 VNNI (on compilers/CPUs that support it).

### Linux

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
```

Pass `-DENABLE_AVX2=ON` to CMake only when building for hardware that supports
the AVX2 and BMI2 instruction sets. The default build avoids these flags so
that the resulting binary runs on older processors that only provide SSE4.1
and POPCNT. `-DENABLE_NATIVE=ON` and `-DENABLE_VNNI=ON` follow the same rule:
enable them only when the target CPUs actually support those instruction sets.

## Run (UCI)

```bash
./SirioC
# then in stdin:
# uci
# isready
# position startpos
# go movetime 100
```

## Features

* Full UCI loop with options (`Hash`, `Threads`, `UseNNUE`, `EvalFile`). Threads
  default to the available hardware concurrency so the engine uses every core
  by default.
* Legal move generation with FEN and `startpos` + `moves` parsing.
* Iterative deepening search backed by a transposition table, killer moves,
  history heuristics, tapered evaluation, and incremental make/unmake to avoid
  copying full board states at every node.
* Quiet-move pruning and search stability refinements inspired by Berserk and
  Stockfish to supply competitive default play.
* Optional Syzygy 5-7-man tablebase probing via the `UseSyzygy` and
  `SyzygyPath` options for perfect endgame information.

## Functional details

### Board representation and move generation

SirioC stores the position in a `Board` class backed by per-piece bitboards,
aggregate occupancy masks, and a 64-entry mailbox array. These redundant views
let the engine answer attack and occupancy queries quickly while still having a
piece-centric representation for NNUE input accumulation. Move generation is
split by piece type into pseudo-legal generators that are filtered for legality
through king-in-check detection. The board tracks castling rights, en passant
files, halfmove counters, and a history stack so that moves can be made and
undone during search without re-parsing FEN strings.

### Search

The searcher implements an iterative deepening negamax with alpha-beta
pruning. A transposition table stores principal variation moves, bounds, and
depth information to guide future visits to the same positions. Classic
move-ordering aids—history and killer heuristics, late move reductions, and
futility pruning—improve convergence. Multi-threading, ponder mode, search
limits (time, depth, nodes), and Syzygy probing are exposed via the UCI loop,
although time management still relies on simple overhead-based budgeting.

### Evaluation

Static evaluation currently relies on tapered piece-square tables derived from
NNUE training material. Material and positional terms are blended between
middlegame and endgame phases, and a small tempo bonus favors the side to move.
The NNUE accumulator infrastructure is wired through the board state, enabling
fast inference once the NNUE evaluator is connected. Until incremental NNUE
updates land, the classical evaluation remains the default. Placeholder
`.nnue` network files are bundled so the UCI options resolve to real files even
though full neural evaluation is not yet implemented.

### Engine influences

The feature set intentionally mirrors proven concepts from Stockfish and
Berserk. Late move reductions, quiet-move pruning, and history heuristics are
lifted from Stockfish-style engines, while futility pruning and bench tooling
borrow from Berserk. This mix yields a familiar, extensible baseline for future
experimentation with NNUE networks and additional heuristics.

## Engine heuristics

SirioC blends ideas from Berserk and Stockfish in the following ways:

* **Late move reductions (Stockfish-inspired):** quiet moves searched later in
  the move ordering are reduced before a verification re-search, helping the
  engine focus on promising branches sooner.
* **History and killer heuristics (Stockfish/Berserk):** quiet moves that cause
  beta cutoffs are rewarded while weak choices are penalised, which improves
  move ordering at all depths.
* **Futility pruning (Berserk-inspired):** shallow quiet moves that cannot
  materially raise alpha are skipped to keep the node count under control.

## TODO (next steps)

* Wire in NNUE accumulator updates and integer inference.
* Add time management utilities, `perft`, and enhanced `bench` modes.

### Search improvement tasks

To keep pace with other competitive UCI engines, future work should continue to
refine the selectivity that now powers SirioC's search:

* Instrument automated tuning for the new pruning margins and reduction tables
  so the enhanced move-count pruning, singular extensions, and beta/ProbCut
  thresholds stay balanced across hardware and time controls.
* Add aspiration windows and a verification re-search path to stabilize root
  move ordering while keeping fail-high/fail-low overhead low.
* Implement improved time management with soft/hard move time budgets and
  search extension throttling to convert search strength into practical play.
* Introduce multi-cut and enhanced futility pruning experiments gated behind
  testing flags so risky selectivity can be evaluated safely.
* Expand the regression/bench suite with mixed tactical and quiet positions so
  new heuristics can be sanity-checked automatically.

## Bench command

SirioC exposes a `bench` command that solves a small suite of tactical
positions at a fixed depth. The output now appears immediately so it can be
captured by GUIs and scripts without issuing an extra command. Example usage:

```bash
bench depth 8
bench depth 6 threads 32
```

If no explicit thread count is supplied, the command uses the current `Threads`
UCI setting. When the option is still at its default value of 1, the engine
automatically expands to the detected hardware concurrency so that bench runs
exercise every core and hyper-thread.
