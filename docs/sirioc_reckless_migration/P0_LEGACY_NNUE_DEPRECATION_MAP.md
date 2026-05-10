# P0-38 — SirioNNUE2 Legacy Pipeline Deprecation Map (No-Behaviour-Change)

## 1) Scope and no-behaviour-change contract
This document is an audit-only deprecation map for legacy SirioNNUE1-era training/export/runtime artifacts.

**This P0-38 deliverable does not change engine behaviour.**
- No runtime loader changes.
- No search/evaluation routing changes.
- No UCI option/default changes.
- No SirioNNUE1 load-path changes.
- No SirioNNUE2 runtime-default changes.
- No legacy file removals.

## 2) Legacy components found
Audit covered:
- `training/nnue/scripts/features.py`
- `training/nnue/scripts/prepare_dataset.py`
- `training/nnue/scripts/train.py`
- `training/nnue/scripts/export_to_engine.py`
- `training/nnue/configs/default.yaml`
- SirioNNUE1 text loader path in runtime NNUE backend/API detection
- `PieceCountModel` class and references
- UCI naming/defaults related to legacy `.nnue` paths (`EvalFile`, `EvalFileSmall`, `NNUEFile`, `UseNNUE`)
- Existing tests that still exercise legacy path/contracts

## 3) Component-by-component deprecation map

### A. `training/nnue/scripts/features.py`
- **Current purpose:** Piece-count feature extraction (`2 x 6` piece slots), simple target shaping (`material + result_weight * outcome`) for legacy training data generation.
- **Role type:** training-only.
- **Track:** **SirioNNUE1 legacy**.
- **Known limitation:** Encodes only coarse piece counts; no sparse king-relative HalfKA-style structure used by SirioNNUE2.
- **Recommended action:** **deprecate later**, then **replace with v2 workflows** once all required pipelines consume v2 feature tooling.
- **Premature-removal risk:** **High** (breaks legacy dataset generation and scripts still documented/usable as baseline).

### B. `training/nnue/scripts/prepare_dataset.py`
- **Current purpose:** Builds `.npz` datasets and metadata from PGNs using legacy piece-count features.
- **Role type:** training-only.
- **Track:** **SirioNNUE1 legacy**.
- **Known limitation:** Output schema/targets are tied to PieceCountModel assumptions, not NNUE2 sparse contract artifacts.
- **Recommended action:** **keep temporarily**, then **deprecate later** after v2 dataset path is confirmed complete for intended workflows.
- **Premature-removal risk:** **High** (breaks existing legacy training preparation path).

### C. `training/nnue/scripts/train.py` (`PieceCountModel`)
- **Current purpose:** Trains legacy linear `PieceCountModel` (`bias`, `scale`, `12` weights) against piece-count datasets.
- **Role type:** training-only.
- **Track:** **SirioNNUE1 legacy**.
- **Known limitation:** Model family matches legacy text format, not SirioNNUE2 architecture/layout contracts.
- **Recommended action:** **remove only after explicit migration** and tests/docs decision, because it remains the canonical legacy baseline trainer.
- **Premature-removal risk:** **High** (breaks legacy baseline reproducibility and exporter dependency).

### D. `training/nnue/scripts/export_to_engine.py`
- **Current purpose:** Exports checkpoints to text `SirioNNUE1` format consumed by legacy loader.
- **Role type:** export-only.
- **Track:** **SirioNNUE1 legacy**.
- **Known limitation:** Text header/weight serialization is specific to legacy loader contract; not a SirioNNUE2 binary export path.
- **Recommended action:** **keep temporarily**, then **deprecate later**; **remove only after explicit migration** once runtime and training workflows no longer rely on SirioNNUE1 artifacts.
- **Premature-removal risk:** **High** (breaks legacy export/load compatibility path).

### E. `training/nnue/configs/default.yaml`
- **Current purpose:** Default training config referencing legacy dataset paths and legacy checkpoint/log destinations.
- **Role type:** config-only.
- **Track:** **SirioNNUE1 legacy**.
- **Known limitation:** Points to legacy training script assumptions and does not encode SirioNNUE2 v2 contract metadata constraints.
- **Recommended action:** **deprecate later** and eventually **replace with v2 config-first workflow defaults**.
- **Premature-removal risk:** **Medium** (breaks default legacy training invocation paths).

### F. Runtime SirioNNUE1 text-network loader (`src/nnue/backend.cpp`)
- **Current purpose:** `SingleNetworkBackend::load(...)` parses `SirioNNUE1` text header and scalar/weight table for legacy runtime evaluation backend.
- **Role type:** runtime.
- **Track:** **SirioNNUE1 legacy runtime baseline**.
- **Known limitation:** Narrow format support by design; not a claim of generic `.nnue` compatibility.
- **Recommended action:** **remove only after explicit migration** decision tied to runtime-default transition and validation gates.
- **Premature-removal risk:** **Critical** (would break current legacy runtime loading behaviour and baseline continuity).

### G. Format detection/API reporting (`src/nnue/api.cpp`)
- **Current purpose:** Detects/labels `SirioNNUE1Legacy` vs `SirioNNUE2MinimalV1`; reports legacy status markers in metadata and diagnostics.
- **Role type:** runtime/support instrumentation.
- **Track:** mixed: legacy detection + v2 detection.
- **Known limitation:** Detection is contract-based and intentionally conservative; non-matching `.nnue` files are not claimed compatible.
- **Recommended action:** **keep temporarily**; evolve only with explicit format-migration decisions.
- **Premature-removal risk:** **High** (reduces safe detection/reporting and could regress migration guardrails).

### H. `PieceCountModel` references outside training (`training/nnue/scripts/evaluate.py`)
- **Current purpose:** Legacy offline evaluation utility imports/uses `PieceCountModel`.
- **Role type:** training/support-only tooling.
- **Track:** **SirioNNUE1 legacy**.
- **Known limitation:** Anchored to legacy model family and metrics assumptions.
- **Recommended action:** **deprecate later** when v2 evaluation tooling fully supersedes legacy workflows.
- **Premature-removal risk:** **Medium** (breaks legacy analysis workflow, not runtime engine path).

### I. UCI naming/defaults with Stockfish-style-looking filenames (`src/uci.cpp`, `include/sirio/uci_options.hpp`)
- **Current purpose:** Exposes `EvalFile`, `EvalFileSmall`, `NNUEFile`, `UseNNUE`; defaults include filenames such as `nn-1c0000000000.nnue` / `nn-37f18f62d772.nnue`.
- **Role type:** runtime configuration surface.
- **Track:** naming compatibility convenience around existing NNUE option surface; not SirioNNUE2 default routing.
- **Known limitation:** Filename style can look Stockfish-like but does not imply binary format compatibility.
- **Recommended action:** **keep temporarily**; any renaming/deprecation should be explicit and coordinated with UCI compatibility expectations.
- **Premature-removal risk:** **High** (breaks user-facing UCI workflows/default expectations).

### J. Legacy-path tests and fixtures
- **Current purpose:** Validate legacy format detection/loading continuity (e.g., `tests/data/minimal.nnue`, backend/format tests, board NNUE fallback coverage).
- **Role type:** test-only.
- **Track:** mostly legacy continuity + mixed compatibility guardrails.
- **Known limitation:** Legacy tests do not validate SirioNNUE2 strength/performance; they validate contracts/safety only.
- **Recommended action:** **keep temporarily** until explicit retirement plan after v2 default migration and replacement test coverage.
- **Premature-removal risk:** **High** (loss of regression detection for legacy continuity promises).

## 4) SirioNNUE1 status statement
**SirioNNUE1 remains legacy/test baseline for now.** It stays available to preserve existing runtime continuity and forensic comparability during NNUE2 migration phases.

## 5) SirioNNUE2 default-status statement
**SirioNNUE2 remains non-default** in runtime/search routing at this stage.

## 6) Stockfish compatibility statement
**Stockfish `.nnue` compatibility is not claimed.** Filename similarity or extension alone is not treated as format compatibility.

## 7) Syzygy artefact policy statement
Physical Syzygy tablebase files (**WDL/DTZ data blobs**) must **not** be committed to this repository. External probing code paths (including third-party integration points) may remain as code references/configuration surfaces.

## 8) Proposed future deprecation sequence (non-binding plan)
1. Mark legacy SirioNNUE1 training/export scripts as deprecated in docs (not runtime behaviour).
2. Ensure SirioNNUE2 v2 candidate/verification pipeline covers all required operational workflows.
3. Add explicit migration warnings/documentation for legacy script usage.
4. Remove legacy scripts/runtime hooks only after:
   - passing replacement tests,
   - explicit roadmap decision,
   - and confirmed non-regression against migration gates.

## 9) Validation commands run
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
- `python tests/nnue_golden_path_v2_smoke_test.py`
- `python tests/nnue_candidate_v2_artifact_test.py`
- `python tests/nnue_candidate_v2_verify_test.py`
- `python tests/nnue_candidate_verified_runtime_load_test.py`

## 10) Elo/strength claim statement
This task is documentation/audit only and makes **no Elo or strength claim**.
