# SirioC

A minimal, buildable skeleton for a UCI chess engine with NNUE scaffolding, inspired by Stockfish and Berserk.

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
* FEN and `startpos` + `moves` parsing to set a position.
* `go` returns `bestmove (none)` until move generation/search are implemented.

## TODO (next steps)

* Implement legal move generation and make/unmake.
* Wire search to produce a legal best move.
* Implement NNUE accumulator updates and integer inference.
* Add `perft` and `bench` modes.
