# SirioC

SirioC is a buildable UCI chess engine skeleton with NNUE scaffolding. Its
search integrates heuristics inspired by the open-source Stockfish and Berserk
projects to provide a stronger baseline playing strength out of the box.

## Build

### Windows (MSYS2 MinGW64 or Clang)
```bash
mkdir build && cd build
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
```

If you know the target machines support AVX2 and BMI2, you can enable the
optimized build with `-DENABLE_AVX2=ON` when configuring CMake.

### Linux

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
```

Pass `-DENABLE_AVX2=ON` to CMake only when building for hardware that supports
the AVX2 and BMI2 instruction sets. The default build avoids these flags so
that the resulting binary runs on older processors that only provide SSE4.1
and POPCNT.

## Run (UCI)

```bash
./SirioC
# then in stdin:
# uci
# isready
# position startpos
# go movetime 100
```

## What works

* Full UCI loop with options (`Hash`, `Threads`, `UseNNUE`, `EvalFile`).
* Legal move generation with FEN and `startpos` + `moves` parsing.
* Iterative deepening search with a transposition table, killer moves, history
  heuristics, and tapered evaluation.
* Quiet-move pruning and search stability refinements inspired by Berserk and
  Stockfish to supply competitive default play.
* Optional Syzygy 5-7-man tablebase probing via the `UseSyzygy` and
  `SyzygyPath` options for perfect endgame information.

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

* Implement incremental make/unmake to avoid copying boards at every node.
* Wire in NNUE accumulator updates and integer inference.
* Add time management utilities, `perft`, and `bench` modes.
