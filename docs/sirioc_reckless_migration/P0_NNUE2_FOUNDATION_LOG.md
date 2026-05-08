# P0-03 SirioNNUE2 Backend Contract / Sparse Feature Foundation

This task introduces the **architectural basis only** for SirioNNUE2 backend work. It does not switch the default evaluation path, does not replace classical fallback, and does not claim strength improvements.

## Files changed
- `include/sirio/nnue/backend.hpp`
- `src/nnue/backend.cpp`
- `src/nnue/api.cpp`
- `tests/nnue_backend_tests.cpp`

## Exact scope
- Added minimal SirioNNUE2 sparse feature contract types and helper interfaces.
- Added NNUE2 binary header metadata contract and default/header-validation helpers.
- Added safe placeholder contracts for sparse incremental updates and accumulator refresh.
- Kept SirioNNUE1 backend path intact and unchanged as default runtime NNUE behavior.
- Exposed SirioNNUE2 experimental format information through NNUE API metadata string.
- Added low-level tests for header contract, sparse container invariants, and default/empty state behavior.

## New structures/types introduced
- `SparseFeature`
- `SparsePerspectiveState`
- `SparseFeatureState`
- `Nnue2Accumulator`
- `Nnue2AccumulatorPair`
- `Nnue2BinaryHeader`
- `Nnue2NetworkParameters`
- constants: `kNnue2PerspectiveCount`, `kNnue2MaxActiveFeatures`, `kNnue2AccumulatorSize`

## Intentionally incomplete (deferred)
- Real NNUE2 feature extractor encoding (full chess feature indexing contract).
- Real incremental sparse update logic bound to move deltas.
- Real accumulator math with NNUE2 weights and activation pipeline.
- Real NNUE2 binary loading/parsing path and runtime evaluator selection.
- Any training pipeline/export changes.

## Backend continuity confirmations
- SirioNNUE2 is **not** the default backend.
- SirioNNUE1 remains available as legacy/runtime baseline.
- Classical evaluation fallback remains unchanged.

## Validation commands run
- `git status --short`
- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
- `cmake --build build -j`
- `ctest --test-dir build --output-on-failure`
- `./build/sirio_tests`
- `./build/sirio_bench`

## Test results
- Build and tests completed successfully in this environment.
- No claim made regarding Elo/strength changes.

## Known limitations
- SirioNNUE2 paths introduced here are contract scaffolding and partial placeholders.
- Sparse update and accumulator routines are not full production NNUE2 yet.
