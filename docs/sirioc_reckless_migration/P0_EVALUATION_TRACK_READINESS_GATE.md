# P0-45 — SirioNNUE2 Stable P0 Evaluation Track Closure / Readiness Gate

## 1) Scope
This deliverable is **documentation-only** and closes the stable P0 SirioNNUE2 evaluation-track state.

- No behaviour change.
- No UCI/search/runtime activation change.
- No SirioNNUE2 default change.
- No search wiring for SirioNNUE2.
- No code-path or test-contract expansion beyond documentation.

## 2) Stable milestones closed
The following stable milestones are recorded as completed in the P0 evaluation track:

- SirioNNUE2 backend foundation.
- SirioHalfKAv1 feature encoder.
- Python/C++ parity.
- Binary export/load contract.
- dataset-v2 scaffold.
- train_v2 path.
- Layout contract.
- C++ loader/inference path.
- Accumulator full-refresh contract.
- Feature diff contract.
- Delta apply contract.
- Transition apply/undo contract.
- En-passant-like closure coverage.
- Board make/unmake shadow harness.
- Internal runtime backend path.
- train-export-runtime golden path.
- Candidate builder.
- Candidate verifier.
- Verified-only runtime load.
- Internal activation candidate.
- Legacy deprecation map/markers.
- Alias audit/reporting.
- Helper failure register.

## 3) Deferred items
The following items remain explicitly deferred:

- P0-37 fixed-FEN corpus report.
- P0-43 static eval comparison.

Reason for deferral:
- Helper/header-contract discrepancy (`Invalid SirioNNUE2 header contract`) in deferred helper/report workflows.

Future retry prerequisite:
- Local path/hash parity diagnostics must prove absolute path, hash, size, argv, cwd, and loader-path parity across successful and failing helper flows before reattempting P0-37/P0-43.

## 4) Current stable branch state
Current stable-state assertions:

- Stable branch remains at **P0-42 runtime/evaluation candidate** for active internal path.
- **P0-36 verified-only runtime load** remains stable.
- **P0-44** records deferred helper failures and diagnostic requirements.
- SirioNNUE2 remains **non-default**.
- No search integration is enabled for SirioNNUE2.
- No public UCI activation is enabled for SirioNNUE2.
- No Elo/strength claim is made.

## 5) Readiness gates before any gauntlet/OpenBench/fastchess
Before any gauntlet or benchmark-match workflow proceeds, all of the following are mandatory:

1. No red tests.
2. Verified candidate load path passes.
3. Internal activation candidate contract passes.
4. Candidate metadata/model-card chain is verified.
5. No fallback on valid candidate path.
6. Explicit no-strength status is retained until actual matches are executed and reviewed.
7. Deferred P0-37/P0-43 helper issue is either locally resolved or explicitly waived with documented rationale.
8. Full test suite is green.
9. Reproducible command list is documented and retained with outputs.

## 6) Readiness gates before search P0 continuation
Before moving to search expansion work, all of the following are mandatory:

1. Current NNUE evaluation track must remain stable.
2. No failed helper work remains unresolved in the active branch scope.
3. Baseline evaluation/default route remains unchanged unless separately approved.
4. Explicit decision recorded: pause NNUE track or proceed to search P0.
5. Search changes must be isolated from unresolved NNUE helper diagnostics.

## 7) Hardware / GPU policy
At current stage:

- No local GPU is required.
- CPU-only minimal candidates are valid for pipeline validation.
- GPU/cloud workflows are deferred to later larger training campaigns.
- No hardware purchase is required for this stage.

## 8) Tablebase / Fathom policy
At current stage:

- `third_party/fathom` warnings are treated as pre-existing and out of current P0 evaluation-track scope.
- Physical Syzygy tablebase files must not be committed.
- Probing code may remain.
- Fathom hygiene can be addressed only as a separate future task if needed.

## 9) Allowed next steps
Allowed next steps after this closure:

- Documentation/closure follow-up tasks.
- Local diagnostics for deferred P0-37/P0-43 helper/header-contract discrepancy.
- Optional UCI-free evaluation-track hardening.
- Only after explicit approval: first local sanity gauntlet preparation.
- Only after explicit approval: search P0 continuation.

## 10) Forbidden next steps without explicit approval
Forbidden without explicit approval:

- Public UCI SirioNNUE2 activation.
- `search.cpp` changes for SirioNNUE2 rollout.
- OpenBench runs.
- fastchess/cutechess match campaigns.
- Elo/strength claims.
- Stockfish `.nnue` compatibility claims.
- GPU-dependent workflow requirements.

## 11) Validation suite (stable command list)
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
- `python tests/nnue_internal_activation_candidate_v2_test.py`

---

This P0-45 closure artifact is documentation-only and preserves existing runtime/search/UCI behaviour.
