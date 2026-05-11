# P0-47 MovePicker Scoring Baseline Audit

- MovePicker baseline audit added.
- Documentation-only patch.
- No search behaviour changed.
- No NNUE behaviour changed.
- SirioNNUE2 remains non-default.
- No strength/Elo claim.

# P0-49 Capture/Noisy History Data Structure Scaffold

- Added capture/noisy history scaffolding in the search-history module with isolated tests.
- No MovePicker/search integration in this step.
- No search behaviour changed.
- No NNUE behaviour changed.
- No strength/Elo claim.

# P0-50 Continuation History Data Structure Scaffold

- Added continuation-history scaffolding in the search-history module with isolated tests.
- No MovePicker/search integration in this step.
- No search behaviour changed.
- No NNUE behaviour changed.
- No strength/Elo claim.

# P0-68 CorrectionHistory Foundation (Zero Runtime Integration)

- Added CorrectionHistory foundation as deterministic storage/API only in search history.
- No NNUE runtime behaviour changed.
- No evaluation backend behaviour changed.
- No strength/Elo claim.

# P0-81 Reverse Futility Margin Helper (Disabled / No-op)

- Added centralized reverse futility margin helper contract in search parameters with deterministic/no-op defaults.
- Reverse futility remains disabled under current defaults (`selectivity_reverse_futility_enabled = false`).
- No NNUE runtime behavior changed.
- No evaluation backend topology changed.
- No strength/Elo claim.


# P0-46 Search P0 Re-Entry Baseline

- Added `docs/sirioc_reckless_migration/P0_SEARCH_REENTRY_BASELINE.md`.
- Documentation-only patch.
- No search behaviour changed.
- No NNUE behaviour changed.
- SirioNNUE2 remains non-default.
- No strength/Elo claim.

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

# P0-12 SirioNNUE2 Non-Default Evaluation Probe / White-POV Contract

This task adds an isolated SirioNNUE2-MinimalV1 evaluation probe contract without routing runtime engine evaluation through NNUE2.

## Files changed
- `include/sirio/nnue/backend.hpp`
- `src/nnue/backend.cpp`
- `tests/nnue_eval_probe_v2_tests.cpp`
- `tests/board_tests.cpp`
- `CMakeLists.txt`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Probe API/function names
- `evaluate_loaded_nnue2_minimal_v1_probe_white_pov(const Board&, const Nnue2NetworkParameters&, std::int32_t&, std::string&)`
- Probe delegates to P0-11 inference helper `evaluate_loaded_nnue2_minimal_v1(...)` and remains non-default.

## White-POV contract
- Probe output is explicitly **White POV**.
- Probe does **not** flip by side to move.
- Side-to-move normalization remains a responsibility of existing `evaluate_for_current_player()` or future integration code.

## Side-to-move invariance tests
- Start position piece placement with white-to-move vs black-to-move returns identical White-POV probe output.
- Kings-only piece placement with white-to-move vs black-to-move returns identical White-POV probe output.
- Deterministic repeated output verified on a simple material FEN.

## Network validation requirements
- Probe requires a validated SirioNNUE2-MinimalV1 network contract.
- Unvalidated/malformed payloads are rejected through existing layout decode and tensor size checks.

## Continuity confirmations
- SirioNNUE2 remains **non-default**.
- Normal `evaluate()` behavior is unchanged.
- `evaluate_for_current_player()` behavior is unchanged.
- Search/UCI/TT/move generation/Syzygy/time management/threading are unchanged.
- No UCI option was added or modified.

## Known limitations
- Probe is an integration contract path only and is not wired into search evaluation routing.
- No strength or Elo claim is made.

## Next deferred step
- Controlled integration design for explicit evaluation routing policy (including side-to-move semantics) while preserving non-regression constraints.


# P0-13 SirioNNUE2 Non-Default Evaluation Adapter / STM-POV Contract

This task adds a strict non-default adapter that converts the existing P0-12 SirioNNUE2 White-POV probe output into side-to-move POV, without changing runtime engine evaluation routing.

## Files changed
- `include/sirio/nnue/backend.hpp`
- `src/nnue/backend.cpp`
- `tests/nnue_eval_probe_v2_tests.cpp`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Adapter API/function names
- Existing probe preserved: `evaluate_loaded_nnue2_minimal_v1_probe_white_pov(...)`
- New non-default adapter: `evaluate_loaded_nnue2_minimal_v1_probe_stm_pov(...)`

## White-POV probe contract
- White-POV probe remains side-to-move invariant.
- Probe value source remains piece placement features only.
- No side-to-move sign normalization is applied in the White-POV probe.

## STM-POV adapter contract
- Adapter calls the existing White-POV probe.
- If side-to-move is White, return White-POV unchanged.
- If side-to-move is Black, return negated White-POV.
- Invalid/unvalidated network remains rejected via existing layout validation path.
- Adapter is isolated and non-default; no hidden global mutable state was added.

## Sign-flip rule
- `stm_pov = white_pov` when `board.side_to_move() == White`
- `stm_pov = -white_pov` when `board.side_to_move() == Black`

## FEN/test cases used
- Start position side-to-move invariance:
  - `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1`
  - `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1`
- Non-zero sign-flip coverage:
  - `4k3/8/8/8/3P4/8/8/4K3 w - - 0 1`
  - `4k3/8/8/8/3P4/8/8/4K3 b - - 0 1`
- Kings-only determinism:
  - `8/8/8/8/8/8/6k1/6K1 w - - 0 1`
  - `8/8/8/8/8/8/6k1/6K1 b - - 0 1`

## Continuity confirmations
- SirioNNUE2 remains non-default.
- Normal `evaluate()` behavior remains unchanged.
- Normal `evaluate_for_current_player()` behavior remains unchanged.
- Search/UCI/TT/movegen/Syzygy/time management/threading remain unchanged.
- No UCI option was added or changed.

## Known limitations
- Adapter remains a controlled probe-only path and is not wired into search runtime evaluation.
- Coverage for `evaluate_for_current_player()` is indirect because it is internal to `search.cpp`.

## Next deferred step
- Controlled wiring decision for optional internal evaluation call-sites (if and only if explicitly authorized in a later roadmap step).

## Originality/provenance note
- Implementation and tests are original SirioC repository code and do not import third-party engine/trainer source.

# P0-14 SirioNNUE2 Experimental Backend Gate / Non-Default Evaluation Routing

This task adds an explicit, non-default experimental backend routing gate for SirioNNUE2 evaluation selection. It does not change default evaluator/search/UCI behavior and does not make any strength claim.

## Files changed
- `include/sirio/nnue/backend.hpp`
- `src/nnue/backend.cpp`
- `tests/nnue_eval_probe_v2_tests.cpp`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Backend gate API added
- `enum class ExperimentalEvalBackend { Classical, ExperimentalSirioNNUE2 }`
- `struct ExperimentalEvalRoutingResult`
- `route_experimental_nnue2_evaluation(...)`

## Default backend behavior
- Default route is preserved as classical by selecting `ExperimentalEvalBackend::Classical`.
- Routing helper returns the provided classical score unchanged when experimental routing is not explicitly selected.

## Experimental backend behavior
- SirioNNUE2 is only attempted when explicitly selecting `ExperimentalEvalBackend::ExperimentalSirioNNUE2`.
- When selected and a valid `SirioNNUE2-MinimalV1` network is supplied, routing calls the existing P0-13 STM-POV adapter (`evaluate_loaded_nnue2_minimal_v1_probe_stm_pov`).

## Missing/invalid network rule
- Contract implemented in this patch: **safe classical fallback**.
- If no network is provided, or the supplied network fails validation/probe execution, routing returns the classical score and marks fallback in `ExperimentalEvalRoutingResult`.

## Tests added
- Default-disabled gate keeps classical score unchanged.
- Explicit experimental gate with valid fixture network matches STM-POV probe output.
- Explicit experimental gate with invalid network falls back to classical.
- Explicit experimental gate with missing network falls back to classical.

## Continuity confirmations
- SirioNNUE2 remains non-default.
- Existing SirioNNUE1 support remains intact.
- Normal search and UCI behavior are unchanged.
- No strength/Elo claim is made.

## Known limitations
- Routing is currently exposed as explicit integration helper, not promoted to a default runtime backend selection policy.
- Fallback diagnostics are plain text and intended for deterministic testing/internal observability.

## Deferred next step
- Wire a controlled internal evaluation-backend selector to this gate in production evaluation flow while preserving non-default status and existing UCI defaults.

## Originality/provenance note
- Implementation and tests are original SirioC repository work and do not import external engine/trainer source.

# P0-15 SirioNNUE2 Evaluation Layer Integration Harness / Default-Off Contract

This task adds a minimal evaluation-layer integration harness that can explicitly route to the P0-14 experimental SirioNNUE2 gate in tests/internal usage while keeping normal evaluation, search, and UCI behavior unchanged.

## Files changed
- `include/sirio/evaluation_route.hpp`
- `src/evaluation_route.cpp`
- `tests/evaluation_route_harness_tests.cpp`
- `tests/board_tests.cpp`
- `CMakeLists.txt`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Harness API/function names
- `enum class EvaluationRoute { DefaultExisting, ExperimentalSirioNNUE2 }`
- `struct EvaluationRouteResult`
- `evaluate_with_experimental_backend_for_tests(const Board&, std::int32_t default_score, EvaluationRoute, const nnue::Nnue2NetworkParameters*, std::string*)`
- `evaluate_with_experimental_backend_for_tests(const Board&, EvaluationRoute, const nnue::Nnue2NetworkParameters*, std::string*)`

## Backend selection names
- `EvaluationRoute::DefaultExisting`
- `EvaluationRoute::ExperimentalSirioNNUE2`

## Default-off contract
- Harness routing remains opt-in and test/internal only.
- When `EvaluationRoute::DefaultExisting` is selected, the provided/default score is returned unchanged.
- No changes were made to the normal `evaluate()` entry point, search routing, or UCI options/defaults.
- SirioNNUE2 remains non-default.

## Fallback rule
- Under `EvaluationRoute::ExperimentalSirioNNUE2`, routing delegates to P0-14 gate behavior.
- Missing network falls back to default score.
- Invalid/rejected network falls back to default score.

## Route metadata contract
- `EvaluationRouteResult::used_default_route`
- `EvaluationRouteResult::used_experimental_route`
- `EvaluationRouteResult::fell_back_to_default`

## Tests added and FENs used
- Default route equals normal evaluate score on:
  - Start position: `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1`
  - Kings-only: `8/8/8/8/8/8/6k1/6K1 w - - 0 1`
  - Asymmetric material/STM case: `4k3/8/8/8/3P4/8/8/4K3 b - - 0 1`
- Explicit experimental route with valid deterministic exported fixture network matches P0-14 routing output.
- Missing network fallback verified.
- Invalid network fallback verified (`hidden_bias` cleared).
- Metadata flags verified for default, successful experimental, and fallback paths.

## Continuity confirmations
- Normal `evaluate()` behavior remains unchanged.
- Search behavior remains unchanged.
- UCI options/defaults remain unchanged.
- SirioNNUE1 legacy support remains unchanged.
- SirioNNUE2 remains non-default.

## Known limitations
- Harness is not yet wired into production evaluation backend policy; it is explicit-call only.
- Diagnostic strings remain intended for deterministic internal/test observability.

## Next deferred step
- Controlled production-side internal selector integration (if authorized), preserving default-off behavior and existing public UCI defaults.

## P0-16 SirioNNUE2 File-Backed Experimental Evaluation Harness / Explicit Net Loading Contract

- **Files changed:**
  - `include/sirio/evaluation_route.hpp`
  - `src/evaluation_route.cpp`
  - `tests/evaluation_route_harness_tests.cpp`
- **Helper API/function names:**
  - `evaluate_with_experimental_backend_file_for_tests(const Board&, std::int32_t, EvaluationRoute, const std::string&, std::string*)`
  - `evaluate_with_experimental_backend_file_for_tests(const Board&, EvaluationRoute, const std::string&, std::string*)`
- **File-loading contract:** file loading is attempted only when `EvaluationRoute::ExperimentalSirioNNUE2` is selected. `DefaultExisting` never attempts load.
- **Validation requirements:** loading uses `load_nnue2_network_file` and execution uses existing `route_experimental_nnue2_evaluation`, which preserves existing SirioNNUE2 header/layout/tensor validation and STM-POV adapter behavior.
- **Fallback rule:** any load failure (missing file, unreadable/truncated/malformed payload, wrong contract header/layout) routes to classical score exactly per P0-14/P0-15 default-off behavior.
- **Metadata contract:** result now exposes selected route, actual route used, file-load attempted/succeeded flags, fallback flag, and fallback reason string.
- **Tests added and fixture policy:** tests build tiny deterministic fixture using existing exporter; malformed/wrong-format/header-mismatch files are tiny temp-directory artifacts only, with no committed large binaries.
- **Non-default confirmation:** SirioNNUE2 remains experimental and default-off.
- **Behavioral invariants:** normal evaluate/search/UCI behavior unchanged; no new UCI options; no search integration.
- **Format compatibility note:** Stockfish `.nnue` compatibility is not claimed and fake `.nnue` content is rejected/fallback.
- **Known limitations:** no runtime production toggle, no network cache, no global mutable file-backed state in this step.
- **Next deferred step:** controlled production wiring and option surface remain deferred beyond P0-16.

# P0-17 SirioNNUE2 Experimental Evaluation Config / Runtime State Contract

## Files changed
- `include/sirio/evaluation_route.hpp`
- `src/evaluation_route.cpp`
- `tests/evaluation_route_harness_tests.cpp`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Config/state type names
- `ExperimentalEvaluationLoadStatus`
- `ExperimentalEvaluationConfig`
- `ExperimentalEvaluationState`
- helper APIs:
  - `prepare_experimental_evaluation_state_for_tests(...)`
  - `evaluate_with_experimental_evaluation_state_for_tests(...)`

## Default-off semantics
- Default construction keeps route at `DefaultExisting`.
- Default construction has no network path, no load attempt, no loaded SirioNNUE2 network.
- Default config evaluation preserves existing classical/default result and does not perform file load.

## Network path/loading contract
- Experimental route requires an explicit network path via `ExperimentalEvaluationConfig::network_path`.
- `prepare_experimental_evaluation_state_for_tests(...)` performs file-backed loading through existing P0-16 loader (`load_nnue2_network_file`).
- Successful load stores a validated `Nnue2NetworkParameters` in state.
- Missing, malformed, or wrong-contract files are rejected with `LoadRejected` status and diagnostic reason.

## State metadata contract
- Route selection is explicit in `ExperimentalEvaluationConfig::selected_route`.
- Load metadata is explicit in state/result fields:
  - `load_attempted`
  - `load_succeeded`
  - `load_status`
  - `fallback_reason`
- Evaluation result continues to expose route/fallback telemetry through `EvaluationRouteResult`.

## Fallback rule
- If route is default: evaluate classically and never load from file.
- If route is experimental but state has no successfully loaded network: hard fallback to classical score with explicit fallback reason.
- If experimental network is loaded, routing reuses existing P0-14/P0-15/P0-16 experimental evaluation path.

## Tests added and FENs used
- Extended `tests/evaluation_route_harness_tests.cpp` with config/state contract coverage.
- Covered FENs:
  - `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1`
  - `8/8/8/8/8/8/6k1/6K1 w - - 0 1`
  - `4k3/8/8/8/3P4/8/8/4K3 b - - 0 1`
- Added checks for default no-load behavior, valid file load success, missing/malformed fallback metadata, file-helper equivalence, and deterministic repeat evaluation.

## Continuity confirmations
- SirioNNUE2 remains non-default.
- Normal `evaluate()` / `evaluate_for_current_player()` behavior is unchanged.
- Search routing is unchanged and no search path is invoked by this contract.
- Public UCI options/defaults are unchanged.
- SirioNNUE1 legacy support remains unchanged.
- No Stockfish `.nnue` compatibility is claimed.
- No Elo/strength claim is made.

## Known limitations
- Contract is test/internal-only and intentionally not exposed as a public UCI activation path.
- No global runtime cache was introduced; state must be prepared explicitly by callers.

## Next deferred step
- Controlled runtime plumbing to pass explicit experimental config/state into an internal evaluation entrypoint while preserving default-off behavior and existing public interfaces.

# P0-18 SirioNNUE2 Accumulator Refresh Baseline / Deterministic Full-Recompute Contract

## Files changed
- `include/sirio/nnue/backend.hpp`
- `src/nnue/backend.cpp`
- `tests/nnue_inference_v2_tests.cpp`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Accumulator type/function names
- `SirioNNUE2MinimalAccumulator`
- `refresh_sirio_nnue2_minimal_accumulator(...)`
- `evaluate_sirio_nnue2_minimal_accumulator(...)`

## Full-refresh formula
- Encode active sparse features via `encode_sirio_halfka_v1(board, sparse)` for both perspectives.
- Initialize accumulator hidden pre-activation vector as `hidden_pre_activation[h] = hidden_bias[h]`.
- For each active feature `f` and hidden index `h`, apply:
  - `hidden_pre_activation[h] += input_weights[f.index * hidden1_size + h] * f.value`.
- Validation rejects uninitialized networks, invalid layout, feature index overflow, or tensor size mismatch.

## Accumulator evaluation formula
- Apply ReLU per hidden element:
  - `activated[h] = max(0, hidden_pre_activation[h])`.
- Output accumulation:
  - `output_accum = output_bias + sum_h(activated[h] * output_weights[h])`.
- Apply test-only quantization normalization consistent with P0-11:
  - divide by `quant_input_scale * quant_output_scale` when denominator > 0.

## Equality contract vs P0-11 minimal inference
- `evaluate_loaded_nnue2_minimal_v1(...)` now routes through full-refresh accumulator + accumulator evaluation.
- Output remains deterministic and equal to prior direct minimal inference contract for tested positions.

## Tests added and FENs used
- Equality/direct-vs-accumulator checks:
  - `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1`
  - `8/8/8/8/8/8/6k1/6K1 w - - 0 1`
  - `4k3/8/8/8/8/8/4P3/4K3 w - - 0 1`
  - `r3k2r/pp3ppp/2n1bn2/3p4/3P4/2N1PN2/PP3PPP/R3K2R w KQkq - 0 1`
- Deterministic repeated refresh check for mixed material FEN.
- Clear/reinitialize semantics check for accumulator state lifecycle.
- Unvalidated network rejection and malformed tensor size rejection.
- Side-to-move invariance for White-POV full-refresh accumulator result on identical placement.

## Continuity confirmations
- Incremental make/unmake accumulator updates are deferred (not implemented in this task).
- SirioNNUE2 remains non-default.
- Normal `evaluate()` / `evaluate_for_current_player()` behavior is unchanged.
- Search routing and UCI defaults/options are unchanged.
- No Elo/strength claim is made.

## Known limitations
- Only full recompute refresh is implemented.
- No incremental update stack or make/unmake delta maintenance yet.

## Next deferred step
- P0-19: incremental accumulator updates with deterministic parity checks against full-refresh baseline before any search integration.

# P0-19 SirioNNUE2 Accumulator Delta Planning / Feature-Diff Contract

This task adds a deterministic SirioHalfKAv1 feature-diff planning contract for before/after board comparison. It does not apply incremental accumulator updates and does not change runtime search/evaluation routing.

## Files changed
- `include/sirio/nnue/backend.hpp`
- `include/sirio/nnue/features.hpp`
- `src/nnue/features.cpp`
- `tests/nnue_feature_diff_v2_tests.cpp`
- `tests/board_tests.cpp`
- `CMakeLists.txt`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Feature-diff API
- `enum class SirioHalfKAv1FullRefreshReason { None, WhiteKingMoved, BlackKingMoved, BothKingsMoved, InvalidInput }`
- `struct SirioHalfKAv1FeatureDiff`
- `bool compute_sirio_halfka_v1_feature_diff(const Board &before, const Board &after, SirioHalfKAv1FeatureDiff &out_diff)`

## Diff semantics
- Encodes both `before` and `after` using `encode_sirio_halfka_v1`.
- Computes deterministic per-perspective set differences:
  - white removed / white added
  - black removed / black added
- Deterministic ordering is enforced by sorting sparse features by `(index, value)` before `std::set_difference`.
- Feature identity includes both `index` and `value`.
- If encoding fails for either board, `full_refresh_required=true` with `InvalidInput`.

## Full-refresh and king-move rule
- `full_refresh_required` is set when either king square changes.
- Reason mapping:
  - white king changed -> `WhiteKingMoved`
  - black king changed -> `BlackKingMoved`
  - both changed -> `BothKingsMoved`
- Even when full refresh is required, the diff lists are still produced when encoding succeeds.
- Incremental accumulator application must remain deferred when `full_refresh_required=true`.

## Ordering and dedup assumptions
- Existing SirioHalfKAv1 encoder contract yields deterministic active feature ordering.
- Diff helper re-sorts each perspective snapshot by `(index, value)` to guarantee deterministic delta lists independent of insertion order.
- No duplicate features are expected from current encoder contract; diff logic remains value-aware and deterministic.

## Tests added and cases covered
- Added `tests/nnue_feature_diff_v2_tests.cpp` and wired into `sirio_tests`.
- Coverage includes:
  - no-change board
  - quiet non-king move + deterministic repeated diff
  - capture-like before/after state
  - promotion-like before/after state
  - castling-like before/after state (king moved => full refresh)
  - king move (full refresh)
  - side-to-move-only change (no feature delta)
  - feature index range `[0, 40960)` and value `1` on all delta lists
- En passant-specific delta case remains deferred as a separate explicit case; current coverage uses safe before/after board states without move-construction fragility.

## Continuity confirmations
- Incremental accumulator update is deferred.
- SirioNNUE2 remains non-default.
- Normal `evaluate()` / `evaluate_for_current_player()` behavior is unchanged.
- Search routing and UCI options/defaults are unchanged.
- SirioNNUE1 legacy support remains unchanged.
- No Elo/strength claims are made.

## Known limitations
- Diff helper is comparison-based planning only; it does not perform in-place accumulator updates.
- `InvalidInput` is surfaced through return `false` + refresh-required reason, but no additional diagnostics are attached.
- En passant-specific legal-move transition coverage is still deferred.

## Deferred next step
- Implement and validate incremental accumulator application using this contract, with make/unmake no-drift checks and king-move forced refresh handling.

# P0-20 SirioNNUE2 Accumulator Delta Apply / Incremental Equals Full-Refresh Contract

## Files changed
- `include/sirio/nnue/backend.hpp`
- `src/nnue/backend.cpp`
- `tests/nnue_accumulator_delta_v2_tests.cpp`
- `tests/board_tests.cpp`
- `CMakeLists.txt`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Delta-apply function names
- `apply_sirio_nnue2_minimal_accumulator_delta(...)`
- Existing full-refresh/eval helpers retained:
  - `refresh_sirio_nnue2_minimal_accumulator(...)`
  - `evaluate_sirio_nnue2_minimal_accumulator(...)`

## Arithmetic formula
For each hidden unit `h`:
- start from existing accumulator pre-activation `A[h]` (already includes `hidden_bias[h]` from refresh)
- for each removed feature `f`: `A[h] -= input_weights[f.index, h] * f.value`
- for each added feature `f`: `A[h] += input_weights[f.index, h] * f.value`

This uses the same integer arithmetic and tensor indexing as full refresh, without resizing/padding/reinterpretation.

## Rejection rule for full_refresh_required
- If `diff.full_refresh_required == true`, delta apply returns `false` with explicit error and does not mutate the accumulator.
- Invalid accumulator state, dimension mismatch, or out-of-range feature index are also rejected without mutation.

## Equality contract versus full-refresh-after
For non-king-changing transitions (`full_refresh_required == false`), tests enforce:
- `refresh(before)` then `apply_delta(before->after)`
- equals `refresh(after)`
for both hidden pre-activation vectors and evaluated output score.

## Tests added and position cases used
Added `tests/nnue_accumulator_delta_v2_tests.cpp` coverage for:
- quiet non-king move: startpos `e2e4`-equivalent FEN transition
- capture-like transition
- promotion-like transition
- side-to-move-only transition (empty diff, accumulator unchanged)
- deterministic repeat delta from same before state
- king-move requires full refresh and is rejected without mutation
- castling-like king move rejected without mutation
- invalid accumulator rejected
- invalid feature diff (out-of-range feature index) rejected without mutation

En-passant-like delta case remains deferred in this step.

## Continuity confirmations
- Make/unmake stack integration is deferred.
- SirioNNUE2 remains non-default.
- Normal evaluate/search/UCI behavior is unchanged.
- No strength/Elo claims are made.

## Known limitations
- Delta path depends on externally computed `SirioHalfKAv1FeatureDiff`; it does not yet integrate with runtime move stack make/unmake.
- En-passant-like incremental contract coverage is deferred.

## Next deferred step
- Wire safe incremental accumulator lifecycle into move make/unmake stack and runtime routing only after maintaining proven full-refresh parity guarantees.

# P0-21 SirioNNUE2 Accumulator Transition Stack / Make-Unmake No-Drift Contract

## Files changed
- `include/sirio/nnue/backend.hpp`
- `src/nnue/backend.cpp`
- `tests/nnue_accumulator_transition_v2_tests.cpp`
- `tests/board_tests.cpp`
- `CMakeLists.txt`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Transition contract names
- `SirioNNUE2MinimalAccumulatorTransition`
- `SirioNNUE2MinimalAccumulatorTransitionStatus`
- `make_sirio_nnue2_minimal_accumulator_transition(...)`
- `apply_sirio_nnue2_minimal_accumulator_transition(...)`
- `undo_sirio_nnue2_minimal_accumulator_transition(...)`

## Apply formula
- Reject transition when `valid == false` or `status != Valid`.
- Reject on invalid accumulator / dimension mismatch.
- Arithmetic (same as P0-20): subtract removed rows, add added rows.
- Transaction rule: compute into a copy and commit only on success.

## Undo formula
- Reject transition when `valid == false` or `status != Valid`.
- Reject on invalid accumulator / dimension mismatch.
- Arithmetic: subtract previously-added rows, add previously-removed rows.
- Transaction rule: compute into a copy and commit only on success.

## No-drift equality contract
- Verified `A -> B -> C -> D` apply chain versus full-refresh at each step.
- Verified reverse undo `D -> C -> B -> A` versus full-refresh at each step.
- All comparisons use exact equality of `hidden_pre_activation` vectors.

## Tests added and cases
- New file: `tests/nnue_accumulator_transition_v2_tests.cpp`
- Cases covered:
  - single transition apply/undo no drift;
  - multi-step chain apply/undo no drift;
  - side-to-move-only no-op apply and no-op undo;
  - king-move rejection with no accumulator mutation;
  - invalid transition data rejection with no mutation;
  - invalid accumulator rejection.
- FEN families used include quiet transitions and chain transitions on fixed king squares.

## Deferred / unchanged confirmations
- Search integration with make/unmake stack is deferred.
- `Board` make/unmake logic is unchanged.
- Normal `evaluate()` / `evaluate_for_current_player()` behavior is unchanged.
- UCI options/defaults are unchanged.
- SirioNNUE2 remains non-default.

## Known limitations
- Transition tests cover deterministic contract behavior only; runtime search stack wiring remains deferred.
- En-passant-like transition case is deferred in this step.

## Next deferred step
- Integrate transition stack into controlled search-time make/unmake path only after isolated no-drift contract is complete.

# P0-22 SirioNNUE2 En-Passant-Like Delta Closure / Captured-Pawn Removal Contract

## Files changed
- `tests/nnue_feature_diff_v2_tests.cpp`
- `tests/nnue_accumulator_delta_v2_tests.cpp`
- `tests/nnue_accumulator_transition_v2_tests.cpp`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Exact scope
- Added before/after board-state tests for en-passant-like captured-pawn removal in SirioHalfKAv1 feature diff.
- Added accumulator delta parity tests (delta-apply equals full refresh after).
- Added transition apply/undo no-drift tests for both white and black en-passant-like transformations.
- Added invalid en-passant-like transition fixture rejection check to confirm no accumulator mutation.
- No move generation, Board make/unmake, search, UCI, or runtime evaluation routing changes.

## FENs used
- White en-passant-like before: `4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1`
- White en-passant-like after: `4k3/8/3P4/8/8/8/8/4K3 b - - 0 1`
- Black mirrored before: `4k3/8/8/8/3Pp3/8/8/4K3 b - d3 0 1`
- Black mirrored after: `4k3/8/8/8/8/3p4/8/4K3 w - - 0 1`

## Feature-diff expectations validated
- `full_refresh_required == false` for both directions.
- Removed/added sparse feature lists are non-empty as expected for mover relocation and captured-pawn disappearance on a different square.
- Side-to-move flips do not alter the feature-diff sets for identical piece-placement transitions.
- Feature indices are bounded in `[0, 40960)` and sparse values remain `1`.

## Accumulator delta equality contract
- For both en-passant-like directions: refresh(before) + delta(before->after) equals refresh(after) for hidden pre-activation state.
- Evaluated score from delta-updated accumulator equals full-refresh accumulator score.

## Transition apply/undo no-drift contract
- For both en-passant-like directions: transition apply equals full-refresh(after), and transition undo returns exactly to full-refresh(before).
- Invalid en-passant-like transition fixture (out-of-range captured-side feature index) is rejected and leaves accumulator unchanged.

## Continuity confirmations
- Board make/unmake and legal en-passant move execution integration remain deferred.
- Search integration remains deferred.
- `evaluate()` and `evaluate_for_current_player()` normal routing is unchanged.
- UCI defaults/options are unchanged.
- SirioNNUE2 remains non-default.

## Known limitations
- Coverage validates before/after board-state delta contracts, not legal-move execution plumbing.
- Runtime/search activation and move-stack integration are still pending later roadmap phases.

## Next deferred step
- Wire these validated special-move delta/transition contracts into controlled runtime/search integration steps after remaining P0 safety gates are complete.

# P0-23 SirioNNUE2 Board Make-Unmake Shadow Harness / Legal Move No-Drift Contract

## Files changed
- `tests/nnue_board_shadow_v2_tests.cpp`
- `tests/board_tests.cpp`
- `CMakeLists.txt`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Board APIs used
- `sirio::generate_legal_moves(const Board&)` for legal move selection.
- `sirio::move_to_uci(const Move&)` for deterministic move matching.
- `Board::make_move(const Move&, UndoState&)` for in-place move application.
- `Board::undo_move(const Move&, const UndoState&)` for in-place rollback.
- `Board::to_fen()` for exact position round-trip verification after undo.

## Move cases tested
- Quiet non-king move: `e2e4` from `4k3/8/8/8/8/8/4P3/4K3 w - - 0 1`.
- Capture: `e5d6` from `4k3/8/8/3pP3/8/8/8/4K3 w - - 0 1`.
- Promotion: `g7g8q` from `4k3/6P1/8/8/8/8/8/4K3 w - - 0 1`.
- En passant: `e5d6` from `4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1`.
- King move requiring refresh/rejection: `e1e2` from `4k3/8/8/8/8/8/8/4K3 w - - 0 1`.
- Castling requiring refresh/rejection: `e1g1` from `4k2r/8/8/8/8/8/8/4K2R w Kk - 0 1`.

## No-drift equality contract
- For non-king-changing legal moves, the harness verifies:
  - transition apply after `Board::make_move` equals full-refresh accumulator(after),
  - transition undo after `Board::undo_move` equals full-refresh accumulator(before),
  - repeated apply/undo cycles do not drift and restore exact baseline each cycle.
- For king-changing legal moves, the harness verifies:
  - transition creation rejects incremental path (`FullRefreshRequired`),
  - incremental apply returns false,
  - accumulator pre-activation vector remains unchanged on rejection.

## Deferred special cases
- None deferred in this step: required move classes were covered using existing legal move generation and existing move/undo APIs.

## Continuity confirmations
- Search integration remains deferred; this step is test-only shadow validation.
- Board move logic was not modified in this patch.
- SirioNNUE2 remains non-default.
- Normal `evaluate()` / search / UCI options/defaults behavior is unchanged.

## Known limitations
- Harness validates transition contract compatibility against Board make/unmake semantics only; it does not activate NNUE2 incremental state inside runtime search nodes.
- Coverage is deterministic for chosen legal positions; broader randomized legal-sequence fuzzing is deferred.

## Next deferred step
- Controlled runtime integration of the validated transition lifecycle into search-node make/unmake state, guarded by full-refresh fallback and parity checks.

# P0-24 SirioNNUE2 Experimental Runtime Backend / Explicit Internal Activation Contract

## Files changed
- `include/sirio/evaluation_route.hpp`
- `src/evaluation_route.cpp`
- `tests/nnue_experimental_runtime_v2_tests.cpp`
- `tests/board_tests.cpp`
- `CMakeLists.txt`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Runtime type/function names
- `ExperimentalSirioNNUE2RuntimeStatus`
- `ExperimentalSirioNNUE2RuntimeResult`
- `ExperimentalSirioNNUE2Runtime`
- `ExperimentalSirioNNUE2Runtime::load_from_config(...)`
- `ExperimentalSirioNNUE2Runtime::load_from_file(...)`
- `ExperimentalSirioNNUE2Runtime::evaluate_with_fallback(...)`

## Activation/load contract
- Runtime defaults to inactive/unloaded with no implicit file loading.
- Activation occurs only via explicit config route selection or explicit `load_from_file`.
- Loading uses existing `load_nnue2_network_file` and required SirioNNUE2-MinimalV1 header/layout contract checks.
- Missing/malformed/wrong-contract files are rejected with explicit `LoadRejected` status and fallback reason.

## Accumulator refresh/evaluation contract
- When active+loaded, evaluation performs explicit `refresh_sirio_nnue2_minimal_accumulator` for the provided `Board`.
- It then evaluates through `evaluate_sirio_nnue2_minimal_accumulator` and converts to STM-POV (`white_pov` negated when side to move is black).
- No Board mutation, no search invocation, and no global evaluation-state mutation are introduced.

## Fallback rule and metadata contract
- If inactive/unloaded/invalid, runtime returns caller-provided default/classical score.
- Result metadata reports whether experimental route was used, whether fallback happened, whether refresh succeeded, runtime status, and fallback reason.

## Tests added and FENs used
- Added `tests/nnue_experimental_runtime_v2_tests.cpp`.
- Covered default inactive fallback, explicit valid load, missing/malformed load failure, deterministic repeated evaluation, and board non-mutation.
- Compared loaded runtime output against existing file-backed harness for:
  - `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1`
  - `8/8/8/8/8/8/6k1/6K1 w - - 0 1`
  - `4k3/8/8/8/3P4/8/8/4K3 b - - 0 1`

## Continuity confirmations
- Search/UCI integration is still deferred.
- SirioNNUE2 remains non-default.
- Normal `evaluate()`, `evaluate_for_current_player()`, search, and UCI option/default behavior remain unchanged.
- No Stockfish `.nnue` compatibility is claimed.

## Known limitations
- Runtime object is internal scaffolding for explicit activation/testing paths only.
- It is not yet wired into production search node lifecycle or UCI runtime selection.

## Next deferred step
- Controlled integration point planning for optional runtime wiring, still guarded by explicit fallback and parity checks, without changing default engine behavior.

# P0-25 SirioNNUE2 Train-Export-Runtime Golden Path / Minimal Network Smoke Contract

## Files changed
- `tests/nnue_golden_path_v2_smoke_test.py`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Exact scope
- Added a deterministic, temporary-directory Python smoke test that proves the full minimal chain:
  dataset-v2 fixture -> `train_v2` checkpoint -> `export_to_engine_v2` engine binary -> `ExperimentalSirioNNUE2Runtime` load/evaluate through the existing C++ runtime smoke helper.
- Reused existing dataset-v2 format, `train_v2.py`, `export_to_engine_v2.py`, C++ loader, and `ExperimentalSirioNNUE2Runtime` contracts.
- Did not alter normal evaluation/search/UCI routing or defaults.

## Test command/path
- Golden-path smoke test command: `python tests/nnue_golden_path_v2_smoke_test.py`
- C++ helper consumed by the test: `build/sirio_nnue_runtime_smoke_contract`

## Generated temporary artifacts
- `tempfile.TemporaryDirectory()` workspace per run containing:
  - `dataset_v2/{train.jsonl,val.jsonl,test.jsonl,MANIFEST.json}`
  - `train_out/checkpoint.pt`
  - `golden_path.nnue2`
- All generated artifacts are ephemeral and are not committed.

## Dataset/trainer/export/runtime chain
1. Build tiny deterministic dataset-v2 rows for three fixed FENs.
2. Train with `training.nnue.scripts.train_v2` for 1 epoch on CPU.
3. Verify checkpoint exists and metadata contract fields:
   - `script_name = training.nnue.scripts.train_v2`
   - `feature_set = SirioHalfKAv1`
   - `features_per_perspective = 40960`
   - `model_layout_name = SirioNNUE2-MinimalV1`
   - `model_layout_version = 1`
4. Export checkpoint using `training.nnue.scripts.export_to_engine_v2`.
5. Load and evaluate via `ExperimentalSirioNNUE2Runtime` through `sirio_nnue_runtime_smoke_contract`.

## FENs evaluated
- Starting position: `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1`
- Kings-only: `8/8/8/8/8/8/6k1/6K1 w - - 0 1`
- Asymmetric material: `4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1`

## Deterministic result contract
- Runtime evaluation is repeated for each FEN and score equality is required between repeated calls.
- For the valid exported network, runtime must remain on the experimental route (`used_experimental_route=true`) with no fallback (`fell_back_to_default=false`).
- FEN text is unchanged before/after runtime evaluation calls.
- Fallback metadata is therefore not used for the valid exported binary.
- Missing/malformed fallback coverage remains in existing runtime/export tests.

## Continuity confirmations
- Generated network is test-only and not competitive; no strength/Elo claim is made.
- SirioNNUE2 remains non-default.
- Normal `evaluate()` / `evaluate_for_current_player()` / search / UCI behavior is unchanged.
- No self-play, teacher engine, OpenBench, fastchess, cutechess, or ORDO workflow was added.

## Known limitations
- The smoke test validates only minimal deterministic integration correctness, not training quality or playing strength.
- It depends on locally built test binaries (`build/sirio_nnue_runtime_smoke_contract`).

## Next deferred step
- Extend deterministic fixture coverage and runtime invariants under controlled gating, while keeping SirioNNUE2 off default search routing.

# P0-26 SirioNNUE2 Evaluation-Layer Shadow Wiring / Explicit Internal Eval Hook Contract

## Files changed
- `include/sirio/evaluation_route.hpp`
- `src/evaluation_route.cpp`
- `tests/nnue_evaluation_shadow_hook_v2_tests.cpp`
- `tests/board_tests.cpp`
- `CMakeLists.txt`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Helper API/function names
- `evaluate_with_sirio_nnue2_runtime_for_tests(...)`
- `ExperimentalSirioNNUE2ShadowEvaluationResult`

## Runtime-to-evaluation-layer contract
- The shadow helper takes a `Board`, a provided default/classical score, and an explicit `ExperimentalSirioNNUE2Runtime` instance.
- It delegates to `ExperimentalSirioNNUE2Runtime::evaluate_with_fallback(...)` and returns score + explicit metadata about route usage and fallback.
- It is internal/test-facing wiring and does not alter search or public UCI routing.

## Fallback rule
- If the provided runtime is inactive/unloaded/rejected (or runtime eval rejects), the helper returns the caller-provided default/classical score and marks fallback metadata.

## Tests added and FENs used
- Added `tests/nnue_evaluation_shadow_hook_v2_tests.cpp` with coverage for:
  - inactive runtime fallback to provided default score,
  - loaded runtime parity versus direct runtime evaluation,
  - deterministic repeated calls,
  - board immutability,
  - unchanged normal `evaluate()` before/after shadow hook calls.
- FENs:
  - `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1`
  - `8/8/8/8/8/8/6k1/6K1 w - - 0 1`
  - `4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1`

## Continuity confirmations
- `evaluate()` and existing public routing are unchanged.
- `evaluate_for_current_player()` behavior remains unchanged (no modifications).
- Search integration remains deferred.
- SirioNNUE2 remains non-default/experimental.
- No Stockfish `.nnue` compatibility claim is made.

## Known limitations
- This task only adds explicit shadow wiring and metadata contract; no search-path activation is included.

## Next deferred step
- Controlled integration planning for optional evaluation routing activation (still gated and non-default).

# P0-27 SirioNNUE2 Evaluation.cpp Shadow Integration / Default Behaviour Preservation Contract

## Files changed
- `include/sirio/evaluation_route.hpp`
- `src/evaluation_route.cpp`
- `tests/evaluation_shadow_integration_v2_tests.cpp`
- `tests/board_tests.cpp`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Evaluation entry points inspected
- `evaluate(...)` in `src/evaluation.cpp`
- `make_nnue_evaluation(...)` overloads in `src/evaluation.cpp`
- `ensure_thread_backend(...)` in `src/evaluation.cpp`
- `initialize_evaluation(...)` in `src/evaluation.cpp`
- `evaluate_for_current_player(...)` in `src/search.cpp` (inspection only)

## Helper API/function names
- `evaluate_with_sirio_nnue2_shadow_integration_for_tests(...)`
- `ExperimentalSirioNNUE2EvaluationShadowIntegrationResult`

## evaluation.cpp touch decision
- `src/evaluation.cpp` was intentionally not modified.
- Reason: existing evaluation core and thread-backend lifecycle are tightly coupled and already proven by broad tests; P0-27 requirement is shadow-only/default-off integration, so a thin evaluation-layer wrapper around the P0-26 hook in `evaluation_route` is the minimal-risk approach.

## Default behaviour preservation proof
- `evaluate_with_sirio_nnue2_shadow_integration_for_tests(...)` delegates to the existing P0-26 runtime hook and is called only by explicit tests.
- No changes were made to `evaluate(...)` routing, backend selection, thread backend lifecycle, or initialization path.
- Dedicated tests assert unchanged `evaluate(...)` outputs for start/kings-only/asymmetric FENs before vs after shadow helper calls.

## Fallback rule
- When runtime is inactive/unloaded/rejected, helper returns caller-provided default/classical score and reports fallback metadata.
- When runtime is loaded, helper returns the same score and route/fallback metadata as P0-26 runtime hook.

## Tests added and FENs used
- Added `tests/evaluation_shadow_integration_v2_tests.cpp` covering:
  - unchanged normal `evaluate(...)` output for:
    - `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1`
    - `8/8/8/8/8/8/6k1/6K1 w - - 0 1`
    - `4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1`
  - inactive runtime fallback to provided default score,
  - loaded runtime parity with P0-26 shadow hook across all three FENs,
  - deterministic repeated calls,
  - board immutability via FEN pre/post equality.

## Continuity confirmations
- Public `evaluate(...)` routing remains unchanged.
- `evaluate_for_current_player(...)` routing/behaviour remains unchanged.
- Search integration is still deferred.
- SirioNNUE2 remains non-default.
- No UCI option/default was added or changed.
- No strength/Elo claim is made.

## Known limitations
- Integration remains test/internal-facing only and requires explicit helper invocation.
- No production search-path activation or UCI-driven route selection is introduced in this step.

## Next deferred step
- Controlled, explicitly gated bridge planning between evaluation runtime hooks and higher-level optional routing, still preserving default classical/legacy behaviour.

# P0-28 SirioNNUE2 Explicit Internal EvalBackend Selector / Default-Off Runtime Wiring

This task adds an explicit internal selector contract for test/internal runtime wiring only. Public UCI options/defaults, search integration, and default evaluation routing remain unchanged.

## Files changed
- `include/sirio/evaluation_route.hpp`
- `src/evaluation_route.cpp`
- `tests/internal_eval_backend_selector_v2_tests.cpp`
- `tests/board_tests.cpp`
- `CMakeLists.txt`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Selector contract
- Types:
  - `InternalEvalBackend`
  - `InternalEvalBackendSelection`
  - `InternalEvalBackendFallbackStatus`
  - `InternalEvalBackendResult`
- Helper:
  - `evaluate_with_internal_eval_backend_for_tests(...)`

## Backend values
- `DefaultExisting`
- `ExperimentalSirioNNUE2`

## Default-off contract
- Default-constructed `InternalEvalBackendSelection` selects `DefaultExisting`.
- Default route returns provided classical/default score.
- Default route does not consult `ExperimentalSirioNNUE2Runtime`.
- SirioNNUE2 remains non-default and internal-only.

## Fallback rule
- If `ExperimentalSirioNNUE2` is requested and runtime is inactive/unloaded, helper falls back to default score.
- Fallback metadata is explicit (`fallback_occurred`, `fallback_status`, `fallback_reason`).

## Metadata contract
`InternalEvalBackendResult` reports:
- requested backend,
- actual backend used,
- whether fallback occurred,
- fallback status/reason,
- whether experimental runtime was consulted,
- whether experimental runtime returned a valid score,
- final score.

## Tests added and FENs used
- Added `tests/internal_eval_backend_selector_v2_tests.cpp`.
- Coverage includes:
  - default-constructed selection uses `DefaultExisting`;
  - `DefaultExisting` returns default score and does not consult runtime;
  - experimental selection falls back with inactive runtime;
  - loaded runtime path matches P0-27 shadow integration helper;
  - determinism across repeated calls;
  - board non-mutation;
  - normal `evaluate()` unchanged for fixed FENs.
- FENs:
  - `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1`
  - `8/8/8/8/8/8/6k1/6K1 w - - 0 1`
  - `4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1`

## Continuity confirmations
- `evaluate()` public/default routing remains unchanged.
- `evaluate_for_current_player()` behavior is preserved indirectly because no public/default evaluation routing changed.
- Search integration remains deferred.
- Public UCI options/defaults are unchanged.
- SirioNNUE1 legacy support remains intact.
- No Stockfish `.nnue` compatibility claim is made.
- No strength/Elo claim is made.

## Known limitations
- Selector helper is internal/test-facing only and not exposed via public UCI.
- Experimental path still depends on explicit runtime load and shadow routing.

## Next deferred step
- Controlled opt-in wiring at later roadmap stage (search/public activation still deferred).

# P0-29 Evaluation.cpp Internal Selector Awareness / No-Route-Change Contract

## Files changed
- `include/sirio/evaluation_route.hpp`
- `src/evaluation_route.cpp`
- `tests/evaluation_internal_selector_v2_tests.cpp`
- `tests/board_tests.cpp`
- `CMakeLists.txt`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Evaluation.cpp entry points inspected
- `make_nnue_evaluation(...)`
- `ensure_thread_backend(...)`
- `initialize_evaluation(...)`
- `evaluate(...)`

## Helper API/function names
- Added `evaluate_with_internal_eval_selector_for_tests(...)`.
- Reused existing `evaluate_with_internal_eval_backend_for_tests(...)` as the delegated selector routing implementation.

## evaluation.cpp touch decision
- `src/evaluation.cpp` was inspected but intentionally not modified to avoid any risk to default routing behavior.
- The new selector-aware helper was implemented in `src/evaluation_route.cpp` adjacent to existing internal test routing helpers.

## Default behavior preservation proof
- The new helper computes default score by calling existing `evaluate(board)` exactly as-is.
- `DefaultExisting` selection returns exactly the same score and route metadata indicating default backend.
- Tests assert unchanged `evaluate()` results across fixed FENs before/after helper usage.

## Selector default/fallback contract
- Selector is internal/test-facing and remains default-off (`DefaultExisting`).
- Experimental selection delegates to existing P0-28 `evaluate_with_internal_eval_backend_for_tests(...)`.
- Inactive or unloaded runtime falls back to classical score with explicit fallback metadata.
- Loaded runtime reports experimental backend usage and remains deterministic.

## Tests added and FENs used
- Added `tests/evaluation_internal_selector_v2_tests.cpp`.
- FENs used:
  1. `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1`
  2. `8/8/8/8/8/8/6k1/6K1 w - - 0 1`
  3. `4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1`
- Coverage includes:
  - normal `evaluate()` unchanged on all three FENs,
  - selector helper with `DefaultExisting` equals `evaluate()`,
  - selector helper with experimental selection and inactive runtime falls back with metadata,
  - selector helper with loaded runtime matches P0-28 backend-selector helper,
  - repeated calls deterministic,
  - board immutability via unchanged FEN snapshots.

## Routing and integration confirmations
- `evaluate()` public behavior unchanged.
- `evaluate_for_current_player()` unchanged indirectly because `src/search.cpp` and default evaluation routing were not edited.
- Search integration remains deferred.
- Public UCI options/defaults unchanged.
- SirioNNUE2 remains non-default and internal-only in this step.
- No Elo/strength claim is made.

## Known limitations
- This step only adds selector-awareness helper glue for internal/test usage.
- No search-thread integration and no public runtime selection exposure are included.

## Next deferred step
- Integrate internal selector decisions into broader evaluation/search plumbing only when explicitly authorized, while preserving fallback safety and UCI contract.

# P0-30 Evaluation.cpp Explicit Experimental Wrapper / Default Path Equivalence Contract

## Files changed
- `include/sirio/evaluation.hpp`
- `src/evaluation.cpp`
- `tests/evaluation_cpp_shadow_wrapper_v2_tests.cpp`
- `tests/board_tests.cpp`
- `CMakeLists.txt`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Evaluation.cpp entry points inspected
- `make_nnue_evaluation(...)`
- `ensure_thread_backend(...)`
- `initialize_evaluation(...)`
- `evaluate(...)`

## Wrapper API/function names
- Added explicit evaluation-layer wrapper:
  - `evaluate_with_experimental_selector_shadow_for_tests(...)`
- Wrapper delegates to existing P0-29 selector-aware helper path via:
  - `evaluate_with_internal_eval_backend_for_tests(...)`

## src/evaluation.cpp touch decision
- `src/evaluation.cpp` was directly touched for this step.
- Added a minimal wrapper implementation colocated with `evaluate(...)`.
- Wrapper is explicit/internal test-facing and default-off.

## Default path equivalence proof
- Wrapper computes default score by calling existing `evaluate(board)` exactly as-is.
- For `InternalEvalBackend::DefaultExisting`, wrapper result score equals `evaluate(board)` exactly.
- Baseline `evaluate()` determinism checks across fixed FENs are unchanged.

## Fallback rule
- For `InternalEvalBackend::ExperimentalSirioNNUE2` with inactive/unloaded runtime, wrapper falls back to default score and returns fallback metadata via `InternalEvalBackendResult`:
  - `actual_backend = DefaultExisting`
  - `fallback_occurred = true`
  - `fallback_status = RuntimeInactiveOrUnloaded`

## Tests added and FENs used
- Added `tests/evaluation_cpp_shadow_wrapper_v2_tests.cpp`.
- FENs covered:
  1. `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1`
  2. `8/8/8/8/8/8/6k1/6K1 w - - 0 1`
  3. `4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1`
- Coverage includes:
  - normal `evaluate()` unchanged on all three FENs,
  - wrapper `DefaultExisting` equals `evaluate()`,
  - wrapper experimental + inactive runtime falls back with metadata,
  - wrapper experimental + loaded runtime matches P0-29 helper,
  - repeated calls deterministic,
  - board not mutated (FEN snapshots unchanged),
  - no search-path invocation (no `src/search.cpp` edits),
  - no UCI option changes.

## Routing and integration confirmations
- `evaluate()` public/default route is unchanged.
- `evaluate_for_current_player()` behavior is preserved indirectly because no search/default route edits were made.
- Search integration remains deferred.
- Public UCI options/defaults are unchanged.
- SirioNNUE2 remains non-default.
- No strength/Elo claim is made.

## Known limitations
- Wrapper is internal/test-facing only and not connected to search or public UCI runtime switching.
- Experimental runtime remains opt-in and dependent on explicit network loading.

## Next deferred step
- Controlled integration point for selector-aware evaluation routing in search plumbing, still default-safe and explicitly authorized.

# P0-31 SirioNNUE2 Evaluation Initialization Shadow Contract / Explicit Runtime Config Load

This task adds an explicit internal/test-facing initialization shadow contract for SirioNNUE2 runtime loading through the evaluation layer. It preserves default runtime behavior and keeps all public routing unchanged.

## Files changed
- `include/sirio/evaluation_route.hpp`
- `src/evaluation_route.cpp`
- `tests/evaluation_initialization_shadow_v2_tests.cpp`
- `tests/board_tests.cpp`
- `CMakeLists.txt`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Evaluation.cpp entry points inspected
- `make_nnue_evaluation(...)`
- `ensure_thread_backend()`
- `initialize_evaluation(...)`
- `evaluate(...)`
- `evaluate_with_experimental_selector_shadow_for_tests(...)`

## Helper API/function names
- `initialize_sirio_nnue2_shadow_runtime_for_tests(const ExperimentalEvaluationConfig&, ExperimentalSirioNNUE2Runtime&)`
- `initialize_sirio_nnue2_shadow_runtime_for_tests(const std::string&, ExperimentalSirioNNUE2Runtime&)`
- `ExperimentalSirioNNUE2RuntimeInitializationResult`

## Whether `src/evaluation.cpp` was touched
- No. `src/evaluation.cpp` was inspected and left unchanged to avoid coupling with normal global/threaded evaluation initialization paths.

## Initialization/load contract
- Default route config (`EvaluationRoute::DefaultExisting`) does not request or attempt load.
- Explicit experimental config/path requests and attempts load via existing `ExperimentalSirioNNUE2Runtime` load contracts.
- No default filename auto-load is performed.
- No global evaluation backend mutation is performed.

## Metadata/fallback contract
`ExperimentalSirioNNUE2RuntimeInitializationResult` reports:
- `load_requested`
- `load_attempted`
- `load_succeeded`
- `runtime_status`
- `fallback_reason`

This captures unloaded/loaded/rejected outcomes while preserving default-off behavior.

## Tests added and FENs used
Added `tests/evaluation_initialization_shadow_v2_tests.cpp` coverage for:
- default config does not load runtime,
- explicit valid tiny SirioNNUE2-MinimalV1 load,
- missing and malformed files reject with metadata,
- deterministic repeated evaluation through P0-30 shadow wrapper,
- board immutability during evaluation,
- normal `initialize_evaluation(...)` and `evaluate(...)` stability.

FENs used:
- `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1`
- `8/8/8/8/8/8/6k1/6K1 w - - 0 1`
- `4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1`

## Continuity confirmations
- `initialize_evaluation(...)` normal behavior unchanged.
- `ensure_thread_backend(...)` normal behavior unchanged.
- `evaluate()` and `evaluate_for_current_player()` public routing unchanged.
- Search integration remains deferred.
- Public UCI options/defaults unchanged.
- SirioNNUE2 remains non-default.
- No strength/Elo claim is made.

## Known limitations
- Initialization helper is internal/test-facing only and does not activate runtime search integration.
- Experimental route still requires explicitly provided SirioNNUE2-MinimalV1-compatible files.

## Next deferred step
- Controlled integration of explicit runtime selection into non-default internal evaluation orchestration, still without changing public UCI defaults or search default routing.

# P0-32 SirioNNUE2 Evaluation API Build Info / Format-Reporting Closure

## Files changed
- `include/sirio/nnue/api.hpp`
- `src/nnue/api.cpp`
- `src/uci.cpp`
- `tests/nnue_api_build_info_v2_tests.cpp`
- `tests/board_tests.cpp`
- `CMakeLists.txt`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## API/build-info function names
- `sirio::nnue::init(...)`
- `sirio::nnue::info()`
- internal `build_info(...)` in `src/nnue/api.cpp`
- `print_loaded_nnue_info(...)` in `src/uci.cpp`

## Reported SirioNNUE2 fields
- support presence marker (`support_present=true` when SirioNNUE2 contract loads)
- `model_layout_name=SirioNNUE2-MinimalV1`
- `model_layout_version=1`
- `feature_set=SirioHalfKAv1`
- `features_per_perspective=40960`
- `accumulator_size=256`
- `hidden1_size=256`
- `hidden2_size=0`
- `output_size=1`
- `activation=relu`
- `binary_magic=SirioNNUE2`
- `binary_version=<header.version>`
- binary section order: `input_weights,hidden_bias,output_weights,output_bias`
- quantization reporting: deterministic placeholder/test scaling currently used, production quantization deferred
- legacy status marker: `legacy_sirio_nnue1_status=legacy_test_baseline`

## Checksum/reporting limitations
- `checksum` is reported from the loaded SirioNNUE2 header field only.
- No checksum is invented when unavailable; fallback/non-NNUE2 path reports `checksum=unavailable`.

## Explicit compatibility/scope statements
- No Stockfish `.nnue` compatibility claim is added.
- No runtime/search/UCI routing change is introduced beyond additional reporting text output.
- SirioNNUE2 remains non-default.
- No Elo or strength claim is made.

## Tests added
- Added `tests/nnue_api_build_info_v2_tests.cpp` with substring assertions for required and forbidden reporting content.

## Known limitations
- Build-info reporting is string-contract based (`NetworkInfo::format_report`) and not a fully structured schema.
- Quantization reporting remains contractual/reporting-only (production quantization pipeline is still deferred).

## Next deferred step
- Move from report-only closure to controlled runtime-integration milestones after separate approval gates.

# P0-33 SirioNNUE2 Explicit Network Format Detection / Legacy-Safe Loader Contract

## Files changed
- `include/sirio/nnue/api.hpp`
- `src/nnue/api.cpp`
- `tests/nnue_format_detection_v2_tests.cpp`
- `tests/board_tests.cpp`
- `CMakeLists.txt`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Detector types/functions
- `NnueNetworkFormat`
- `NnueNetworkFormatInfo`
- `detect_nnue_network_format(const std::string& path)`

## Classification values
- `Unknown`
- `SirioNNUE1Legacy`
- `SirioNNUE2MinimalV1`
- `Malformed`
- `Unsupported`

## Detection rules
- Detection is content-based; filename alone is never considered authoritative.
- SirioNNUE2 detection requires successful binary header parsing plus `SirioNNUE2-MinimalV1` layout decode.
- SirioNNUE1 legacy detection requires text header `SirioNNUE1` and complete legacy parameter table parsing.
- Missing/unreadable files return safe `Unknown` with diagnostics.
- Truncated/broken NNUE2 payload/header contracts return `Malformed`.
- Non-matching binaries (including fake `.nnue` naming/content) are classified as non-compatible (`Unknown`/`Malformed`/`Unsupported` as applicable).

## Fake Stockfish `.nnue` rejection rule
- Detection must not claim compatibility from `.nnue` naming.
- Fake Stockfish-style filenames/content are explicitly validated as non-Sirio formats and are never reported as Sirio-compatible.
- No Stockfish `.nnue` compatibility is claimed.

## SirioNNUE1 legacy handling
- Legacy SirioNNUE1 remains supported by explicit safe parsing checks.
- Existing runtime default and legacy continuity are unchanged.

## Tests added / fixtures
- Added `tests/nnue_format_detection_v2_tests.cpp`.
- Coverage includes:
  - valid tiny SirioNNUE2-MinimalV1 binary detection (exported fixture);
  - malformed binary safe rejection;
  - missing file safe classification;
  - fake Stockfish-style `.nnue` name/content non-compatibility;
  - existing tiny SirioNNUE1 fixture (`tests/data/minimal.nnue`) legacy detection;
  - detector non-mutation of global NNUE runtime load state.

## Continuity confirmations
- Runtime/search/UCI/evaluation behaviour is unchanged.
- SirioNNUE2 remains non-default.
- No Stockfish `.nnue` compatibility is claimed.
- No Elo/strength claim is made.

## Known limitations
- Unsupported SirioNNUE2 variants beyond MinimalV1 are currently classified but not activated.
- Detector diagnostics are string-based and intentionally minimal for now.

## Next deferred step
- P0-34 should consume detector metadata in controlled internal reporting paths while still keeping runtime routing and public UCI exposure unchanged.

# P0-34 SirioNNUE2 Minimal Functional Net Candidate / Reproducible Artifact Contract

This step adds an additive reproducible artifact-builder contract for a minimal SirioNNUE2 candidate (dataset-v2 -> train_v2 checkpoint -> export_to_engine_v2 net -> metadata/checksums). It is artifact-only and does not activate SirioNNUE2 in normal runtime routing.

## Files changed
- `training/nnue/scripts/build_candidate_v2.py`
- `training/nnue/configs/sirio_nnue2_candidate_minimal.yaml`
- `tests/nnue_candidate_v2_artifact_test.py`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Helper script path
- `training/nnue/scripts/build_candidate_v2.py`

## Config path
- `training/nnue/configs/sirio_nnue2_candidate_minimal.yaml` (additive; `training/nnue/configs/default.yaml` left unchanged)

## Generated artifact names
- `checkpoint.pt`
- `candidate.nnue2`
- `CANDIDATE_MANIFEST.json`
- `MODEL_CARD.json`

## Manifest/model-card fields
- Script identity/version and generated timestamp.
- Feature/layout contract fields (`feature_set`, `features_per_perspective`, `model_layout_name`, `model_layout_version`).
- Model shape/activation contract.
- Training parameters (`epochs`, `batch_size`, `learning_rate`, `seed`, `device`).
- Dataset path and dataset `MANIFEST.json` hash when present.
- Export mode/stats from `export_to_engine_v2`.
- Artifact hashes for checkpoint and engine binary.
- Explicit statements: test/minimal candidate only, no Elo/strength claim; SirioNNUE2 remains non-default.

## Checksum policy
- SHA-256 is recorded for `checkpoint.pt`, `candidate.nnue2`, and dataset `MANIFEST.json` when available.

## CPU / no-GPU policy
- Helper defaults to `--device cpu` and supports `auto` only as optional override.
- No GPU is required by the candidate contract.

## Deterministic limitations
- For fixed dataset ordering and seed, checkpoint and exported candidate hashes are stable in this environment.
- `generated_at_utc` is intentionally non-deterministic metadata.

## Tests added
- `tests/nnue_candidate_v2_artifact_test.py`
  - Creates deterministic tiny dataset-v2 fixture in a temp dir.
  - Runs candidate builder on CPU with tiny parameters.
  - Asserts presence of checkpoint/net/manifest/model-card artifacts.
  - Verifies required metadata fields and hash correctness.
  - Verifies runtime acceptance path using existing NNUE runtime smoke contract binary.
  - Verifies hash stability across two fixed-seed runs.

## Continuity confirmations
- SirioNNUE2 remains non-default.
- Normal evaluate/search/UCI behavior remains unchanged.
- No integration with self-play/teacher engines/OpenBench/fastchess/cutechess/ORDO added.
- No strength claim is made.

## Known limitations
- This is still a minimal functional candidate artifact flow, not production search integration.
- Format detection assertion is covered through existing runtime smoke acceptance path in this test workflow.

## Next deferred step
- Promote from candidate artifact reproducibility toward controlled runtime gating/instrumentation (still non-default) with additional compatibility and safety tests.

# P0-35 SirioNNUE2 Candidate Manifest Runtime Verification / Artifact Integrity Contract

This task adds deterministic candidate-artifact verification before any SirioNNUE2 runtime-use workflow. Scope is integrity/traceability only.

## Files changed
- `training/nnue/scripts/verify_candidate_v2.py`
- `tests/nnue_candidate_v2_verify_test.py`
- `tests/nnue_format_detect_contract.cpp`
- `CMakeLists.txt`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Verification helper path
- `training/nnue/scripts/verify_candidate_v2.py`

## Manifest/model-card fields verified
- manifest: `feature_set=SirioHalfKAv1`
- manifest: `features_per_perspective=40960`
- manifest: `model_layout_name=SirioNNUE2-MinimalV1`
- manifest: `model_layout_version=1`
- manifest: no-Elo/no-strength statement (`candidate_intent`)
- manifest: non-default statement (`sirio_nnue2_default_status`)
- model card: no-strength statement (`status`)
- model card: non-default statement (`non_default_confirmation`)
- model card `training` and `dataset` blocks must match manifest blocks

## Hash policy
- Computes SHA-256 of `candidate.nnue2` and compares with manifest `artifacts.candidate.sha256`.
- Computes SHA-256 of `checkpoint.pt` (when present) and compares with manifest `artifacts.checkpoint.sha256`.
- Missing required files and hash mismatches are hard failures.

## Format detection policy
- Reuses existing P0-33 detector by adding a minimal helper binary:
  - `sirio_nnue_format_detect_contract`
  - Calls `sirio::nnue::detect_nnue_network_format(...)`
- Python verifier requires detected format `SirioNNUE2MinimalV1`; any other format fails.

## Failure cases tested
- corrupted `candidate.nnue2` bytes
- corrupted manifest candidate hash
- missing `MODEL_CARD.json`
- fake Stockfish-style payload not compatible with Sirio format contract

## CPU/no-GPU confirmation
- Verification and tests are CPU-only and do not require GPU.

## Continuity confirmations
- SirioNNUE2 remains non-default.
- Normal `evaluate()`, `initialize_evaluation()`, `ensure_thread_backend()`, and `evaluate_for_current_player()` behavior remains unchanged.
- Search/UCI routing and public UCI defaults/options are unchanged.
- No self-play, teacher engines, OpenBench, fastchess, cutechess, or ORDO integration was added.
- No Elo/strength claim is made.

## Known limitations
- Format verification shell-outs to the local helper binary under `build/`; helper must be built first.
- Current no-strength/non-default checks are explicit text-contract checks and intentionally strict.

## Next deferred step
- Integrate this verifier as a mandatory precondition gate in future candidate activation/gauntlet orchestration scripts (outside this task scope).

# P0-36 SirioNNUE2 Candidate Runtime Load From Verified Manifest / Verified-Only Contract

This task adds an internal/test-facing verified-only runtime-load contract: candidate directory -> manifest/model-card/hash/format verification -> runtime load smoke. It does not alter public activation, search wiring, or UCI defaults.

## Files changed
- `training/nnue/scripts/verified_runtime_load_v2.py`
- `tests/nnue_candidate_verified_runtime_load_test.py`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Helper/test path
- Helper: `python -m training.nnue.scripts.verified_runtime_load_v2 --candidate-dir <dir>`
- Test: `python tests/nnue_candidate_verified_runtime_load_test.py`

## Verification-before-load rule
- The helper always runs `verify_candidate_v2.py` first against the candidate directory contract (`candidate.nnue2`, `CANDIDATE_MANIFEST.json`, `MODEL_CARD.json`).
- Runtime load is attempted only if verification succeeds (including manifest/model-card integrity, candidate SHA-256, `SirioNNUE2MinimalV1` format, no-strength language, and non-default confirmations enforced by P0-35 verifier).

## Metadata contract
- `verification_attempted`
- `verification_succeeded`
- `load_attempted`
- `load_succeeded`
- `failure_reason`
- `candidate_path`
- `manifest_path`

## Failure cases tested
- Corrupted `candidate.nnue2` content (hash mismatch) -> refused before runtime load.
- Corrupted candidate hash in `CANDIDATE_MANIFEST.json` -> refused before runtime load.
- Missing `MODEL_CARD.json` -> refused before runtime load.
- Fake Stockfish-style payload in `candidate.nnue2` -> refused by verification before runtime load.

## Runtime behavior confirmations
- CPU-only flow; no GPU required by this contract.
- SirioNNUE2 remains non-default.
- Normal `evaluate()`, initialization, routing, search, and UCI behavior remain unchanged.
- No self-play, teacher engines, fastchess/cutechess/ORDO/OpenBench integration added.
- No Elo/strength claim is made.

## Known limitations
- Contract is internal/test-facing and currently exercised through smoke/runtime harness binaries rather than production routing.
- Helper reports pass/fail metadata and does not mutate global evaluation state.

## Next deferred step
- Controlled integration point for verified candidate selection into broader experimental harness flow (still non-default and non-public), preserving verification gate as mandatory precondition.

## Originality/provenance note
- Implementation and tests are original SirioC repository work and do not import external engine/trainer source code.

## P0-37R Revert/Quarantine Failed Corpus Report Work

- P0-37 fixed-FEN corpus report workflow is deferred.
- Reason: the corpus helper path failed to load an otherwise verified candidate with `Invalid SirioNNUE2 header contract`.
- Stable branch continuity remains at P0-36 verified-only candidate runtime load.
- No validation contracts were weakened by this revert/quarantine action.
- No search/UCI/evaluation-default behavior changed.
- SirioNNUE2 remains non-default.
- Deferred corpus reporting should be revisited later with explicit local path/hash parity diagnostics between verified-load and corpus helper paths.

# P0-38 SirioNNUE2 Legacy Pipeline Deprecation Map / No-Behaviour-Change Contract

## Files changed
- `docs/sirioc_reckless_migration/P0_LEGACY_NNUE_DEPRECATION_MAP.md`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Scope
- Added a legacy SirioNNUE1/PieceCount deprecation map documenting current roles, risks, and phased retirement recommendations.
- Audit/documentation only; no runtime/search/UCI/training-script code behaviour changes.

## Link
- See `docs/sirioc_reckless_migration/P0_LEGACY_NNUE_DEPRECATION_MAP.md`.

## Continuity confirmations
- SirioNNUE1 remains legacy/test baseline.
- SirioNNUE2 remains non-default.
- No Stockfish `.nnue` compatibility claim is made.
- No Elo/strength claim is made.


# P0-39 Legacy NNUE Deprecation Markers / Docs-Only User-Facing Warning Contract

This task adds non-behaviour-changing deprecation markers for the legacy
SirioNNUE1/PieceCountModel training/export path so contributors do not mistake
it for the primary NNUE development flow.

## Files changed
- `training/nnue/scripts/features.py`
- `training/nnue/scripts/prepare_dataset.py`
- `training/nnue/scripts/train.py`
- `training/nnue/scripts/export_to_engine.py`
- `training/nnue/configs/default.yaml`
- `training/nnue/README.md`
- `docs/sirioc_reckless_migration/P0_LEGACY_NNUE_DEPRECATION_MAP.md`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Scope
- Added comments/documentation only.
- Legacy pipeline remains available for compatibility/test baseline.
- Directed new work to v2 scripts (`*_v2.py`, candidate build/verify/runtime-load scripts).

## Continuity confirmations
- No executable logic changes.
- No import/CLI/script-output changes.
- No runtime evaluation/search/UCI behaviour changes.
- SirioNNUE2 remains non-default.
- No Stockfish `.nnue` compatibility claim is introduced.
- No Elo/strength claim is made.


## P0-40 SirioNNUE2 Legacy Runtime Alias Audit / No-Behaviour-Change Contract

- Added `docs/sirioc_reckless_migration/P0_RUNTIME_NNUE_ALIAS_AUDIT.md` with a precise audit of runtime-visible NNUE option names, aliases, default filenames, and reporting surfaces.
- Confirmed this step is documentation-only and does not change UCI options/defaults, loader semantics, search routing, or evaluation behaviour.
- Reaffirmed that SirioC does not claim Stockfish `.nnue` compatibility and that SirioNNUE2 remains non-default.

# P0-41 SirioNNUE2 Runtime Alias Reporting Clarification / No-Default-Change Contract

This task adds reporting-only NNUE clarification strings. It does not change loaders, runtime defaults, UCI option names, or search/evaluation routing.

## Files changed
- `src/nnue/api.cpp`
- `tests/nnue_api_build_info_v2_tests.cpp`
- `docs/sirioc_reckless_migration/P0_RUNTIME_NNUE_ALIAS_AUDIT.md`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Reporting strings/fields added
- `stockfish_nnue_compatibility=not_claimed`
- `sirio_nnue1_nnue_names=legacy_sirio_format`
- `sirio_nnue2_runtime_status=non_default`

These are emitted through existing `NetworkInfo::format_report` surfaces only.

## Confirmations
- No UCI option name/default changed (`UseNNUE`, `EvalFile`, `EvalFileSmall`, `NNUEFile` unchanged).
- No loader/runtime behaviour changed.
- SirioNNUE2 remains non-default.
- No Stockfish `.nnue` compatibility is claimed.
- No strength/Elo claim is made.

## Known limitations
- Clarification is metadata/reporting-only; it does not enforce migration policy.
- Legacy `.nnue` naming surfaces remain runtime-visible by design in this stage.

## Next deferred step
- Consider a later approved stage to add canonical/legacy labeling in user-facing UCI docs and eventual alias deprecation planning, without changing runtime behaviour until explicitly authorized.

# P0-42 SirioNNUE2 Internal Evaluation Activation Candidate / Default-Off Contract

## Files changed
- `CMakeLists.txt`
- `tests/sirio_nnue_internal_activation_contract.cpp`
- `tests/nnue_internal_activation_candidate_v2_test.py`
- `docs/sirioc_reckless_migration/P0_NNUE2_FOUNDATION_LOG.md`

## Helper/test path
- Test-only helper binary: `build/sirio_nnue_internal_activation_contract`
- Python contract test: `tests/nnue_internal_activation_candidate_v2_test.py`

## Activation flow
1. Build a temporary NNUE2 candidate artifact with `build_candidate_v2.py`.
2. Require candidate verification via `verify_candidate_v2.py`.
3. Reconfirm verified runtime contract via `verified_runtime_load_v2.py`.
4. Load the verified `candidate.nnue2` in `ExperimentalSirioNNUE2Runtime` through the internal helper.
5. Evaluate fixed FENs through `evaluate_with_experimental_selector_shadow_for_tests(...)` under explicit backend selection.
6. Confirm deterministic result and no board mutation.

## Verified-candidate-before-activation rule
- Activation candidate flow is gated by verification first.
- The Python contract verifies the candidate and verified-runtime-load contract before invoking internal activation helper.
- Missing/tampered/unverified candidate paths are rejected and do not produce experimental activation.

## Backend selector behavior
- `InternalEvalBackend::DefaultExisting` returns baseline `evaluate()` score and reports actual backend `DefaultExisting`.
- Explicit `InternalEvalBackend::ExperimentalSirioNNUE2` with verified candidate reports actual backend `ExperimentalSirioNNUE2` and non-fallback.

## FENs tested
- Start position: `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1`
- Kings-only: `8/8/8/8/8/8/6k1/6K1 w - - 0 1`
- Asymmetric material: `4k3/8/8/3q4/4N3/8/8/4K3 w - - 0 1`

## Fallback/refusal cases
- Tampered candidate payload (`candidate.nnue2` appended bytes) -> load rejected.
- Missing candidate path -> load rejected.

## Continuity confirmations
- SirioNNUE2 remains non-default.
- Normal `evaluate()`, `initialize_evaluation()`, `ensure_thread_backend()`, `evaluate_for_current_player()`, search, and UCI option/default behavior remain unchanged.
- No engine-vs-engine matches, no OpenBench/fastchess/cutechess/ORDO, and no self-play/teacher integration were added.
- No strength/Elo claim is made.

## Known limitations
- Activation candidate is internal/test-only and performed through a contract helper binary rather than public UCI controls.
- The helper consumes a file path and emits contract lines; it is not integrated into search or production routing.

## Next deferred step
- After roadmap approval, add a controlled non-default runtime activation surface that still preserves mandatory verified-candidate gating and no-default-change policy.

# P0-43R Revert/Quarantine Failed Static Eval Compare Work

- P0-43 static evaluation comparison is deferred.
- Reason: static compare helper failed to load an otherwise verified runtime artifact with `Invalid SirioNNUE2 header contract`.
- Stable branch state remains at P0-42 internal activation candidate workflow.
- No validation was weakened.
- No search/UCI/evaluation-default behavior changed.
- SirioNNUE2 remains non-default.
- Deferred static comparison should be revisited only after the helper/header-contract discrepancy is diagnosed locally.

# P0-44 SirioNNUE2 Helper Contract Failure Register / Deferred Diagnostics Map

## Summary
- Added `docs/sirioc_reckless_migration/P0_HELPER_CONTRACT_FAILURE_REGISTER.md` as a docs-only forensic register for deferred helper-contract failures.
- No code, runtime, search, evaluation, UCI, or helper-behaviour changes were made.
- P0-37 and P0-43 remain deferred/quarantined pending local path/hash parity diagnostics.

## Stability confirmations
- Stable branch remains at P0-42.
- SirioNNUE2 remains non-default.
- No strength/Elo claim is made.

# P0-45 — Stable P0 Evaluation Track Closure / Readiness Gate

This milestone adds `docs/sirioc_reckless_migration/P0_EVALUATION_TRACK_READINESS_GATE.md` as a documentation-only closure artifact for the stable SirioNNUE2 P0 evaluation track.

## Scope
- Documentation only.
- No behaviour change.
- No runtime/UCI/search activation change.
- SirioNNUE2 remains non-default.
- No Elo/strength claim.

## Link
- `docs/sirioc_reckless_migration/P0_EVALUATION_TRACK_READINESS_GATE.md`

# P0-48 MovePicker Ordering Snapshot Harness

- Added deterministic MovePicker ordering snapshot harness in `tests/move_picker_snapshot_tests.cpp`.
- Added minimal search-local test adapter to invoke current production MovePicker ordering without refactoring search.
- No search behaviour changed.
- No NNUE behaviour changed.
- SirioNNUE2 default/runtime behaviour unchanged.
- No strength/Elo claim.

# P0-51 Correction History Data Structure Scaffold

- Added correction-history scaffolding in the search-history module with isolated tests.
- No MovePicker/search/evaluation integration in this step.
- No search behaviour changed.
- No evaluation behaviour changed.
- No NNUE behaviour changed.
- No strength/Elo claim.

# P0-52 SearchHistory Aggregate Lifecycle Audit

- Added aggregate `SearchHistory` lifecycle tests covering quiet/killer/capture/noisy/continuation/correction state.
- No search behavior changed.
- No NNUE behavior changed.
- No strength/Elo claim.

# P0-53 Capture/Noisy History Key Extraction Contract

- Added deterministic capture/noisy history key extraction helpers and key structs in the history module with isolated tests.
- No MovePicker/search integration in this step.
- No search behaviour changed.
- No NNUE behaviour changed.
- No strength/Elo claim.

# P0-54 Continuation History Key Extraction Contract

- Added deterministic continuation-history key extraction contract helpers with isolated tests.
- No MovePicker/search integration in this step.
- No search behaviour changed.
- No NNUE behaviour changed.
- No strength/Elo claim.

# P0-55 Correction History Key Extraction Contract

- Added deterministic correction-history key extraction contract helpers.
- No search behaviour changed.
- No evaluation behaviour changed.
- No NNUE behaviour changed.
- No strength/Elo claim.

# P0-56 SearchHistory Key Contract Aggregate Audit

- Added aggregate SearchHistory key-contract audit coverage across capture/noisy, continuation, and correction key families.
- No search behaviour changed.
- No NNUE behaviour changed.
- No strength/Elo claim.

# P0-57 MovePicker History Integration Decision Matrix

- MovePicker history integration decision matrix added.
- Documentation-only patch.
- No search behaviour changed.
- No NNUE behaviour changed.
- No strength/Elo claim.

# P0-58 Capture/NoisyHistory Read-Only MovePicker Scoring

- Added CaptureHistory/NoisyHistory read-only scoring in MovePicker tactical ordering.
- Search behaviour change is limited to this single ordering surface.
- No NNUE behaviour changed.
- No strength/Elo claim.

# P0-59 Capture/NoisyHistory Search Update-Policy Scaffold

- Added capture/noisy update-policy scaffold helper(s) and isolated tests.
- No real search runtime integration in negamax/quiescence yet.
- MovePicker scoring remains unchanged from P0-58 in this step.
- No search behaviour claim beyond isolated helper/test scaffold.
- No NNUE behaviour changed.
- No strength/Elo claim.

# P0-60 Capture/Noisy Search-Update Shadow Harness (Test-Only)

- Added deterministic capture/noisy search-update shadow harness for tests only.
- Harness applies P0-59 update-policy decisions to `SearchHistory` from explicit simulated events.
- No `negamax` integration added.
- No `qsearch` integration added.
- No runtime search behaviour changed.
- No NNUE behaviour changed.
- No strength/Elo claim.

# P0-61 Capture/NoisyHistory Runtime Update Integration-Point Audit

- Added `docs/sirioc_reckless_migration/P0_CAPTURE_NOISY_RUNTIME_UPDATE_AUDIT.md`.
- Documentation-only patch.
- No search behaviour changed.
- No NNUE behaviour changed.
- No strength/Elo claim.

# P0-62 Capture/NoisyHistory Negamax Tactical Beta-Cutoff Runtime Update

- Added one explicit runtime search update point for Capture/NoisyHistory: main negamax tactical beta-cutoff only.
- Search behaviour changed only at that explicit cutoff integration point.
- No qsearch tactical update path was added.
- No continuation-history or correction-history runtime integration was added.
- No NNUE runtime behaviour was changed.
- No strength/Elo claim is made.

# P0-63 Capture/Noisy runtime update observability (no NNUE/search behavioural expansion)

- Added deterministic observability for the existing P0-62 capture/noisy runtime update contract.
- Scope is test/inspection only: counters + constrained helper validation for allowed/excluded update sites.
- No new search update points were introduced beyond P0-62.
- No qsearch or failed tactical capture/noisy updates were introduced.
- No continuation/correction integration was introduced.
- No NNUE behaviour changed.
- No strength/Elo claim.

# P0-64 ContinuationHistory Read-Only MovePicker Quiet Scoring Hook

- Added a read-only ContinuationHistory contribution to MovePicker quiet scoring.
- No ContinuationHistory runtime update path was added in search.
- Capture/NoisyHistory P0-62/P0-63 runtime-update path is unchanged.
- No NNUE runtime behavior change.
- No strength/Elo claim.

# P0-65 ContinuationHistory quiet beta-cutoff runtime update

- Added a single ContinuationHistory runtime update point for main-negamax quiet beta-cutoff only.
- Added deterministic test-observability counters/helpers for this path.
- No NNUE runtime behaviour was changed.
- No evaluation backend behaviour was changed.
- No strength/Elo claim.

# P0-66 ContinuationHistory Quiet Cutoff Symmetry
- Added conservative ContinuationHistory quiet-cutoff malus for previously searched quiet moves at the same main-negamax cutoff node.
- No NNUE runtime behaviour changed.
- No evaluation backend change.
- No strength/Elo claim.

# P0-67 Capture/NoisyHistory Read-Only Tactical MovePicker Hook

- Added a read-only Capture/NoisyHistory tactical scoring helper in MovePicker for captures/en-passant/promotions.
- No NNUE runtime behavior changed.
- No evaluation backend behavior changed.
- No strength/Elo claim is made.

# P0-69 CorrectionHistory read-only static-eval helper / deferred runtime key wiring

- Added a read-only CorrectionHistory static-eval correction helper in the history module.
- No NNUE runtime behavior changed.
- No evaluation backend topology changed.
- Runtime main-search wiring is intentionally deferred until a safe board-derived CorrectionHistory runtime key contract exists.
- No strength/Elo claim.

# P0-70 CorrectionHistory Runtime Key Contract Foundation (No Consumption)

- Added a deterministic CorrectionHistory runtime key construction contract via board-derived helper (`side_to_move` + pawn-occupancy-derived bucket).
- Integration remains foundation-only: no runtime CorrectionHistory consumption/update in search, qsearch, evaluation, pruning, LMR, MovePicker, TT, or UCI.
- No NNUE runtime behavior changed.
- No evaluation backend topology changed.
- No strength/Elo claim.

# P0-71 CorrectionHistory Read-Only Main Negamax Static-Eval Wiring

- Added read-only CorrectionHistory consumption in main negamax static evaluation path.
- No NNUE runtime behaviour changed.
- No evaluation backend topology changed.
- No strength/Elo claim.

# P0-72 CorrectionHistory Quiet Beta-Cutoff Runtime Update
- Added one CorrectionHistory runtime update point for main negamax quiet beta-cutoff only.
- No NNUE runtime behavior changed.
- No evaluation backend topology changed.
- No strength/Elo claim.

# P0-73 CorrectionHistory Runtime Helper API Boundary Cleanup

- CorrectionHistory quiet beta-cutoff runtime helper API boundary was cleaned.
- Production search no longer calls a `*_for_tests` helper.
- No NNUE runtime behaviour changed.
- No evaluation backend topology changed.
- No strength/Elo claim.

# P0-74 CorrectionHistory Main Negamax Fail-Low Negative Runtime Update

- Added CorrectionHistory main negamax fail-low negative runtime update.
- No NNUE runtime behavior changed.
- No evaluation backend topology changed.
- No strength/Elo claim.

# P0-75 CorrectionHistory Runtime Delta Scaling / Saturation Hardening

- Added centralized CorrectionHistory runtime delta scaling/clamping for existing runtime update helpers.
- Applied to both quiet beta-cutoff positive and fail-low negative CorrectionHistory runtime updates.
- No NNUE runtime behavior changed.
- No evaluation backend topology changed.
- No strength/Elo claim.

# P0-76 Search Selectivity Parameters Foundation / Zero-Behaviour Contract

- Added centralized search-selectivity parameter foundation with disabled/no-op defaults for reverse futility, move count pruning, probcut, and singular extensions.
- No NNUE runtime behaviour changed.
- No evaluation backend topology changed.
- No strength/Elo claim.

# P0-77 Reverse Futility Decision Helper Foundation (Disabled/No-Op)

- Added reverse futility helper foundation with disabled/no-op behaviour.
- Helper is gated by the centralized P0-76 selectivity disabled flag.
- No reverse futility pruning return is active in search.
- No NNUE runtime behaviour changed.
- No evaluation backend topology changed.
- No strength/Elo claim.

# P0-78 Reverse Futility Disabled Main-Negamax Probe Wiring

- Added reverse futility helper probe wiring in main negamax as a disabled no-op call-site.
- No NNUE runtime behaviour changed.
- No evaluation backend topology changed.
- No strength/Elo claim.

# P0-79 Reverse Futility Disabled Guarded Return Scaffold

- Added reverse futility disabled guarded return scaffold in main `negamax(...)`.
- No NNUE runtime behaviour changed.
- No evaluation backend topology changed.
- No strength/Elo claim.

# P0-80 Reverse Futility Safety Predicate Expansion (Disabled/No-Op)

- Reverse futility safety predicate expansion was added with explicit safety gating and disabled/no-op behaviour under defaults.
- No NNUE runtime behaviour changed.
- No evaluation backend topology changed.
- No strength/Elo claim.

# P0-82 Reverse Futility Return Observability (Disabled / No-op)

- Added reverse futility guarded-return observability with disabled/no-op behaviour.
- No NNUE runtime behaviour changed.
- No evaluation backend topology changed.
- No strength/Elo claim.

# P0-83 Reverse Futility Conservative Activation (Main Negamax)
- Reverse futility pruning enabled under conservative main-negamax guards.
- No NNUE runtime behaviour changed.
- No evaluation backend topology changed.
- No strength/Elo claim.

# P0-84 Move Count Pruning Helper Foundation (Disabled / No-op)

- Added move count pruning helper foundation with disabled/no-op behavior.
- No NNUE runtime behavior changed.
- No evaluation backend topology changed.
- No strength/Elo claim.

# P0-85 Move Count Pruning Disabled Main-Negamax Probe Wiring

- Move Count Pruning disabled main-negamax probe wiring was added.
- No NNUE runtime behavior changed.
- No evaluation backend topology changed.
- No strength/Elo claim.


# P0-86 Move Count Pruning Disabled Guarded Continue Scaffold

- Added a Move Count Pruning disabled guarded continue scaffold in main negamax.
- No NNUE runtime behavior changed.
- No evaluation backend topology changed.
- No strength/Elo claim.

# P0-87 Move Count Pruning Continue Observability (Disabled/No-op)

- Added Move Count Pruning guarded-continue observability in main negamax while keeping MCP disabled/no-op by default.
- No NNUE runtime behavior changed.
- No evaluation backend topology changed.
- No strength/Elo claim.
