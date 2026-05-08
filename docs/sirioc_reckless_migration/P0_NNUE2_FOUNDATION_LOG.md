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

# P0-04 SirioNNUE2 Sparse Feature Encoder v1

This task adds the first deterministic sparse feature-index contract for SirioNNUE2 and keeps evaluation routing unchanged.

## Feature set
- Name/id: `SirioHalfKAv1`
- Perspectives: 2 (white, black)
- Relative channels per perspective: 10
- Squares: 64 king squares and 64 piece squares
- Features per perspective: `64 * 10 * 64 = 40960`
- Exact index formula: `((perspective_king_square * 10 + relative_piece_channel) * 64 + perspective_piece_square)`

## Perspective transform rule
- White perspective uses board squares as-is.
- Black perspective uses a documented vertical flip normalization (`rank -> 7 - rank`, file unchanged).

## Relative channel mapping
- 0 own pawn
- 1 own knight
- 2 own bishop
- 3 own rook
- 4 own queen
- 5 enemy pawn
- 6 enemy knight
- 7 enemy bishop
- 8 enemy rook
- 9 enemy queen

## Files changed
- `include/sirio/nnue/features.hpp`
- `src/nnue/features.cpp`
- `tests/nnue_features_tests.cpp`
- `tests/board_tests.cpp`
- `CMakeLists.txt`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Tests added
- Added dedicated `nnue_features_tests` coverage for constants, start position active-count contract, kings-only behavior, exact index checks, transform determinism, duplicate-index prevention, and state-clear behavior.

## Continuity confirmations
- SirioNNUE2 is still **not** default.
- Evaluation routing is unchanged.
- Search/UCI/TT/movegen/time-management behavior is unchanged.
- Training/export pipeline is unchanged.

## Deferred items
- Python parity encoder.
- Binary exporter.
- Real accumulator update math.
- Threat-side channel.
- Input buckets.
- Output buckets.
- Trainer integration.

## Originality/provenance note
- This feature-index contract and implementation are original SirioC code and do not import third-party engine/trainer source.

# P0-05 SirioHalfKAv1 Python Parity Encoder

This task adds a deterministic Python SirioHalfKAv1 encoder and a parity harness against the existing C++ P0-04 contract, with no evaluation/search/UCI routing changes.

## Python module added/changed
- Added `training/nnue/scripts/features_v2.py` with explicit SirioHalfKAv1 constants, FEN parsing/validation, perspective transform helpers, and sparse active-index encoding for both perspectives.

## C++ helper/test path added/changed
- Added `tests/nnue_feature_dump.cpp`.
- Added `sirio_feature_dump` target to `CMakeLists.txt`.
- Added deterministic parity script `tests/nnue_feature_parity_test.py` that compares Python output with C++ dump output.

## Feature-set constants
- perspective count: 2
- relative channel count: 10
- square count: 64
- features per perspective: 40960

## Index formula
- `((perspective_king_square * 10 + relative_piece_channel) * 64 + perspective_piece_square)`

## Perspective transform rule
- White perspective: identity.
- Black perspective: vertical rank flip (`rank -> 7 - rank`, same file).

## FEN test cases used
1. `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1`
2. `8/8/8/8/8/8/4k3/4K3 w - - 0 1`
3. `8/8/8/3k4/8/8/4P3/4K3 w - - 0 1`
4. `4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1`

## Parity method
- Build and run `sirio_feature_dump` for fixed FEN inputs.
- Parse deterministic `P0/P1` sparse feature index/value lines.
- Compute Python sparse outputs for the same FENs.
- Assert exact equality of active counts, index lists, values, and ordering.

## Continuity confirmations
- SirioNNUE2 is still **not** default.
- Evaluation routing is unchanged.
- Trainer/exporter/incremental accumulator work is still deferred.

## Known limitations
- The parity harness is a deterministic fixed-FEN contract check only.
- No binary exporter, dataset pipeline rewrite, or trainer integration is introduced in this task.

## Originality/provenance note
- Implementation and tests are original SirioC repository work and do not import third-party engine/trainer source.

# P0-06 SirioNNUE2 Binary Format / Exporter / Loader Roundtrip

This task introduces a deterministic SirioNNUE2 binary contract, a Python v2 exporter, and a C++ loader/validator roundtrip harness. It does not enable SirioNNUE2 for runtime evaluation.

## Binary magic/version
- Magic: `SirioNNUE2\0\0`
- Version: `2`

## Feature set id
- `1` = `SirioHalfKAv1`

## Header fields
- magic (12 bytes)
- version (uint16)
- feature_set_id (uint16)
- flags (uint16)
- features_per_perspective (uint32)
- perspective_count (uint32)
- accumulator_size (uint32)
- hidden_dimensions (uint32)
- output_dimensions (uint32)
- quant_input_scale (uint32)
- quant_output_scale (uint32)
- input_weights_bytes (uint32)
- hidden_bias_bytes (uint32)
- output_weights_bytes (uint32)
- output_bias_bytes (uint32)
- payload_bytes (uint32)
- checksum (uint32; reserved)

## Payload layout
1. input_weights (int16 array)
2. hidden_bias (int16 array)
3. output_weights (int16 array)
4. output_bias (int32)

## Files changed
- `include/sirio/nnue/backend.hpp`
- `src/nnue/backend.cpp`
- `training/nnue/scripts/export_to_engine_v2.py`
- `tests/nnue_roundtrip_tests.cpp`
- `tests/board_tests.cpp`
- `CMakeLists.txt`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Exporter path
- `training/nnue/scripts/export_to_engine_v2.py`

## Loader path
- `load_nnue2_network_file(...)` in `src/nnue/backend.cpp`

## Tests added
- `tests/nnue_roundtrip_tests.cpp` with deterministic export/load and byte-identical re-export checks.

## Rejection cases covered
- wrong magic
- truncated file
- incompatible feature count
- incompatible version

## Continuity confirmations
- SirioNNUE2 is still **not** default.
- Evaluation routing remains unchanged.
- Search/UCI/TT/movegen/time-management behavior remains unchanged.
- Trainer work remains deferred.

## Known limitations
- Exported network is deterministic dummy payload for binary contract validation only.
- No real training semantics or runtime NNUE2 eval path are introduced in this step.
- No Stockfish `.nnue` compatibility claim is made.

## Originality/provenance note
- Implementation is original SirioC code and does not import third-party engine/trainer source.
