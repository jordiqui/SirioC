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

# P0-07 SirioNNUE2 Dataset v2 Scaffold

This task adds a deterministic dataset-v2 scaffold for SirioNNUE2 training preparation. It converts FEN-based records into sparse SirioHalfKAv1 feature records and split JSONL datasets, without changing engine runtime behavior.

## New script path
- `training/nnue/scripts/prepare_dataset_v2.py`

## Input format
- Supported deterministic text format: TSV/whitespace records with six-field FEN + `score_cp` + `result`, optional `source`.
- Supported optional JSONL format with `fen`, `score_cp`, `result`, optional `source`.
- `--format auto|tsv|jsonl` controls parsing mode.

## Output format
- Deterministic output directory containing:
  - `train.jsonl`
  - `val.jsonl`
  - `test.jsonl`
  - `MANIFEST.json`
- Each record includes: `fen`, `features.white`, `features.black`, `score_cp`, `result`, `wdl`, `phase`, `source`, `feature_set`.

## Label contract
- `score_cp` is stored and preserved as **White POV**.
- `result` is kept verbatim and mapped to deterministic `wdl`:
  - `1-0 -> 1.0`
  - `1/2-1/2 -> 0.5`
  - `0-1 -> 0.0`
  - `* -> null`
- This scaffold does not switch score POV by side-to-move.

## Feature contract
- Uses `training/nnue/scripts/features_v2.py` SirioHalfKAv1 sparse encoder.
- Encodes both white and black perspectives.
- Enforces deterministic ordering.
- Rejects invalid FENs, invalid king counts, out-of-range feature indices, and non-unit feature values.

## Phase/material bucket rule
- Deterministic non-king material bucket from FEN piece placement:
  - `opening` for material >= 40
  - `middlegame` for material >= 15 and < 40
  - `endgame` for material < 15
- Bucket is a simple scaffold stratification signal and is not competitively tuned.

## Split method
- Accepted records are sorted by deterministic SHA-256 key of `seed|fen`.
- Split sizes are ratio-based (`--val-ratio`, `--test-ratio`) and deterministic for fixed seed/input.
- `MANIFEST.json` records counts, ratios, seed, accepted/rejected totals, and rejection summary.

## Tests added
- `tests/nnue_dataset_v2_test.py` covers:
  - end-to-end dataset/manifest generation
  - start-position 30/30 sparse feature counts
  - kings-only 0/0 sparse feature counts
  - invalid FEN rejection accounting
  - missing king rejection accounting
  - White-POV `score_cp` preservation
  - deterministic WDL mapping
  - deterministic split stability for same seed
  - feature index range and unit value checks
  - required manifest metadata presence

## Continuity confirmations
- No teacher engine execution is added.
- No self-play generation is added.
- No full trainer implementation is added.
- SirioNNUE2 is still **not** default.
- No C++ evaluation/search/UCI/runtime routing behavior is changed.

## Known limitations
- Input schema is intentionally minimal and strict.
- `*` result maps to `null` WDL and is not imputed.
- No QAT/trainer loop/teacher distillation is part of this patch.

## Originality/provenance note
- Implementation and tests are original SirioC repository work and do not import third-party engine/trainer source.

# P0-08 SirioNNUE2 Minimal Trainer v2 / Checkpoint Contract

This task adds an additive trainer-v2 scaffold for SirioNNUE2 dataset-v2 files and deterministic checkpoint generation. It is pipeline-validation work only and is not a competitive network.

## Trainer script path
- `training/nnue/scripts/train_v2.py`
- Optional minimal config: `training/nnue/configs/sirio_nnue2_minimal.yaml`

## Dataset input contract
- Reads `train.jsonl`, `val.jsonl`, `test.jsonl`, and `MANIFEST.json` from a dataset-v2 directory.
- Validates `feature_set == SirioHalfKAv1` per record.
- Validates every sparse feature pair as `[index, value]` with `0 <= index < 40960` and `value == 1`.
- Treats `score_cp` as White-POV target.
- Loads only the score target into loss; WDL remains in records for future extensions.
- Fails clearly on malformed records or unsupported feature data.

## Model architecture summary
- Minimal sparse scaffold:
  - shared embedding table over `features_per_perspective = 40960`
  - sum active embedding rows separately for white/black feature lists
  - concatenate white/black accumulators
  - small dense head `Linear -> ReLU -> Linear`
  - scalar output (centipawn-like regression target)
- Uses accumulator size `256` in line with existing SirioNNUE2 backend contract constants.

## Target contract
- Training target: `score_cp` interpreted as **White POV**.

## Checkpoint format
- PyTorch `.pt` checkpoint containing:
  - `state_dict`
  - `metadata`
  - `model_config`
  - per-epoch loss logs

## Metadata fields
- `feature_set`
- `features_per_perspective`
- `target_contract`
- `model_architecture`
- `dataset_manifest_path`
- `dataset_manifest_sha256`
- `seed`
- `epochs`
- `batch_size`
- `learning_rate`
- `script_name`
- `script_version`
- deterministic timestamp placeholder keyed by seed

## Tests added
- `tests/nnue_train_v2_test.py` covering tiny deterministic CPU training and contract checks:
  1. tiny dataset-v2 fixture training for 2 epochs,
  2. checkpoint creation,
  3. metadata `feature_set == SirioHalfKAv1`,
  4. metadata `features_per_perspective == 40960`,
  5. malformed feature-set rejection,
  6. out-of-range feature-index rejection,
  7. stable metadata for identical seed runs,
  8. no dependency on legacy `train.py` path,
  9. no engine binary/evaluation routing invocation in trainer tests.

## Continuity confirmations
- This is **not** a competitive network implementation and makes no Elo/strength claim.
- No teacher-engine distillation logic was added or executed.
- No self-play generation was added or executed.
- SirioNNUE2 remains non-default in engine runtime.
- Export-to-engine/runtime evaluation routing remains deferred for trained checkpoints.

## Known limitations
- Architecture is intentionally minimal and CPU-safe for contract testing only.
- Loss uses only `score_cp`; WDL/multi-target training is deferred.
- No quantization-aware training or production tuning is included.

## Originality/provenance note
- Implementation and tests are original SirioC repository work and do not vendor third-party trainer code.

# P0-09 SirioNNUE2 Checkpoint-to-Binary Export Bridge

This task extends the SirioNNUE2 v2 exporter with a validated checkpoint bridge path while preserving the P0-06 deterministic dummy export path and keeping runtime engine behavior unchanged.

## Exporter path
- `training/nnue/scripts/export_to_engine_v2.py`

## New CLI options
- `--checkpoint <path>`: validate a `train_v2.py` checkpoint for SirioNNUE2 binary export compatibility.
- `--output <path>` remains required.
- `--describe` remains available.

## Supported modes
- Mode A: deterministic dummy export (unchanged P0-06 behavior).
- Mode B: checkpoint validation/export bridge entrypoint via `--checkpoint`.

## Checkpoint metadata requirements
The bridge now requires all of:
- top-level `metadata`
- top-level `model_config`
- top-level `state_dict`
- `metadata.feature_set == SirioHalfKAv1`
- `metadata.features_per_perspective == 40960`
- `metadata.script_name == training.nnue.scripts.train_v2` (rejects legacy/non-v2 checkpoints)
- expected model keys present in `state_dict`:
  - `embedding.weight`
  - `head.0.weight`
  - `head.0.bias`
  - `head.2.weight`
  - `head.2.bias`

## Mapping decision
- Actual checkpoint weight mapping is **safely deferred** for now.
- Reason: current `train_v2.py` minimal architecture (`embedding -> concat(white,black) -> Linear(512->32) -> ReLU -> Linear(32->1)`) does not match P0-06 SirioNNUE2 payload contract sections (`input_weights`, `hidden_bias[256]`, `output_weights[256]`, `output_bias`) without undocumented transformations.
- Exporter now fails explicitly with clear incompatibility diagnostics rather than emitting partial/unsafe binaries.

## Binary compatibility validation
- P0-06 dummy export binary contract remains unchanged and loadable through existing C++ loader path `load_nnue2_network_file(...)`.
- Header contract remains:
  - magic `SirioNNUE2\0\0`
  - version `2`
  - feature_set_id `1`
  - features_per_perspective `40960`

## Tests added
- `tests/nnue_export_v2_test.py`.

## Rejection cases covered
- checkpoint `feature_set` mismatch
- missing required metadata
- incompatible/dimension-mismatched tensor layout
- incompatible architecture mapping to v2 payload sections

## Continuity confirmations
- SirioNNUE2 remains **non-default**.
- C++ evaluation routing is unchanged.
- Search/evaluation/TT/UCI/movegen/time-management are unchanged.
- No self-play, teacher-engine, OpenBench, or fastchess integration was added.

## Known limitations
- Checkpoint bridge currently validates and rejects incompatible layouts; it does not yet emit trained-weight SirioNNUE2 binaries.
- Production quantization/mapping policy for a compatible trainer layout remains future work.
- No strength/Elo/official-network claims are made.

## Originality/provenance note
- Changes are original SirioC repository work and do not import third-party engine/trainer source.

# P0-10 SirioNNUE2 Unified Model Layout Contract / Trainer-Binary Alignment

- model layout: `SirioNNUE2-MinimalV1`, version `1`.
- feature set: `SirioHalfKAv1`, `features_per_perspective=40960`, little-endian binary fields.
- dimensions: `accumulator_size=256`, `hidden1_size=256`, `hidden2_size=0 (deferred placeholder)`, `output_size=1`.
- activation: `relu`.
- tensor names/order: `input_embedding.weight`, `hidden.bias`, `output.weight`, `output.bias`.
- binary section order: `input_weights`, `hidden_bias`, `output_weights`, `output_bias`.
- checkpoint metadata contract now includes script name, feature contract, model layout name/version, seed, dataset manifest hash/path, epochs, batch size, learning rate.
- exporter validates metadata/model_config/state_dict presence, script/feature/layout compatibility, required tensor names and exact shapes before writing.
- checkpoint exports now map deterministically to the existing SirioNNUE2 container without reshape/pad/truncate.
- quantization/scaling status: deterministic placeholder integer rounding with fixed `quant_input_scale=256` and `quant_output_scale=256`; production-grade quantization remains deferred.
- tests updated for checkpoint metadata coverage and exporter compatibility/rejection cases.
- dummy export path remains unchanged and deterministic.
- SirioNNUE2 remains non-default; C++ evaluation routing/search behavior unchanged.
- deferred next step: production quantization/training semantics and runtime NNUE2 inference path.

# P0-10B SirioNNUE2 Unified Layout Validation Closure

- Coverage restoration: **yes**. `tests/nnue_export_v2_test.py` was tightened to restore explicit header checks and deterministic C++ loader-path validation requirements.
- Dummy export continuity: deterministic dummy export remains byte-identical across repeated exports.
- Checkpoint continuity: `train_v2` checkpoint with `SirioNNUE2-MinimalV1` exports successfully.
- C++ acceptance: exported checkpoint path is validated alongside built `build/sirio_tests` `[nnue_roundtrip]` coverage; loader acceptance path is exercised through the real built test binary.
- Deterministic binary location: `build/sirio_tests` (asserted by the Python export-v2 test before roundtrip invocation).

## Validation commands run
- `git status --short`
- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
- `cmake --build build -j`
- `ctest --test-dir build --output-on-failure`
- `./build/sirio_tests`
- `./build/sirio_bench`
- `python -m compileall training/nnue/scripts`
- `python tests/nnue_feature_parity_test.py`
- `python tests/nnue_dataset_v2_test.py`
- `python tests/nnue_train_v2_test.py`
- `python tests/nnue_export_v2_test.py`

## Results summary
- C++ configure/build: passed.
- ctest: passed (`sirio_board_tests`).
- `./build/sirio_tests`: passed.
- `./build/sirio_bench`: passed.
- Python parity/dataset/train/export tests: all passed.

## Explicit confirmations
- Exported checkpoint binary compatibility is accepted by the C++ SirioNNUE2 loader/roundtrip validation path.
- Dummy export remains byte-identical.
- SirioNNUE2 remains non-default.
- Engine evaluation/search routing behavior is unchanged.

## Known limitations
- Production-grade quantization remains deferred; current path remains deterministic placeholder quantization/scaling.
- This closure validates layout/export/loader contract alignment; it does not introduce runtime SirioNNUE2 evaluation routing.

# P0-11 SirioNNUE2 C++ Minimal Inference / Loaded-Network Evaluation Contract

This task adds a deterministic, test-only C++ inference path for loaded `SirioNNUE2-MinimalV1` binaries while keeping engine runtime routing unchanged.

## Files changed
- `include/sirio/nnue/backend.hpp`
- `src/nnue/backend.cpp`
- `tests/nnue_inference_v2_tests.cpp`
- `tests/board_tests.cpp`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Decoded layout contract
- `model_layout_name`: `SirioNNUE2-MinimalV1` (decoded contract label)
- `model_layout_version`: `1`
- `feature_set`: `SirioHalfKAv1`
- `features_per_perspective`: `40960`
- `accumulator_size`: `256`
- `hidden1_size`: `256`
- `hidden2_size`: `0` placeholder
- `output_size`: `1`
- `activation`: `relu`

## Tensor/section interpretation (P0-10 order)
1. `input_weights` (`int16`, `[features_per_perspective, hidden1_size]`)
2. `hidden_bias` (`int16`, `[hidden1_size]`)
3. `output_weights` (`int16`, `[hidden1_size]`)
4. `output_bias` (`int32`, scalar)

The decoder performs explicit size checks and rejects malformed tensors instead of truncating/padding.

## Inference formula (test-only minimal contract)
- Encode active sparse features via existing C++ `encode_sirio_halfka_v1(...)` for both perspectives.
- Hidden pre-activation per neuron `h`:
  - `pre[h] = hidden_bias[h] + sum_{active features f from both perspectives}(input_weights[f, h] * f.value)`
- Activation:
  - `act[h] = max(0, pre[h])` (ReLU)
- Output accumulator:
  - `raw = output_bias + sum_h(act[h] * output_weights[h])`
- Returned scalar:
  - If `quant_input_scale * quant_output_scale > 0`, return `raw / (quant_input_scale * quant_output_scale)` as integer division.
  - Otherwise return unscaled `raw`.

## Scaling / quantization status
- Scaling is explicitly marked as **test-only inference scaling** based on current placeholder quantization fields in the exported v2 binary.
- No production-strength quantization calibration is claimed in this step.

## Tests added
- Added `tests/nnue_inference_v2_tests.cpp` with:
  - layout decode contract checks,
  - deterministic repeat-eval checks,
  - malformed section-size rejection.

## FEN cases tested
- Kings-only: `8/8/8/8/8/8/6k1/6K1 w - - 0 1`
- Starting position: `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1`
- Simple material: `4k3/8/8/8/8/8/4P3/4K3 w - - 0 1`

## Continuity confirmations
- SirioNNUE2 remains non-default.
- Evaluation/search/UCI routing is unchanged.
- Legacy SirioNNUE1 runtime behavior is unchanged.
- No strength/Elo claim is made.

## Known limitations
- This is deterministic contract validation, not production NNUE2 integration.
- Inference currently assumes fixed `SirioNNUE2-MinimalV1` dimensions and one-output topology.
- Quantization/scaling semantics remain placeholder-level until later roadmap steps.

## Next deferred step
- Wire validated NNUE2 inference into a guarded runtime path only after finalizing quantization semantics, incremental accumulators, and explicit backend-selection policy gates.
