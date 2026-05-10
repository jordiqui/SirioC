# P0-44 — SirioNNUE2 Helper Contract Failure Register / Deferred Diagnostics Map

## 1) Scope
This deliverable is **documentation-only** and records deferred helper-contract failures for forensic continuity.

- No behaviour changes.
- No helper restoration.
- No validation weakening.
- No runtime, search, or UCI routing changes.

## 2) Stable state
- Stable branch remains at **P0-42 internal activation candidate** state.
- **P0-36 verified-only candidate runtime load** remains stable.
- **P0-37 fixed-FEN corpus reporting** remains deferred.
- **P0-43 static evaluation comparison** remains deferred.
- SirioNNUE2 remains **non-default**.
- Normal evaluation/search/UCI behaviour remains unchanged.

## 3) Known-good paths
The following workflows are retained as known-good stable paths:

| Path | What it validates | Loads `candidate.nnue2` successfully | Retained in stable branch |
|---|---|---|---|
| `training/nnue/scripts/build_candidate_v2.py` | Produces candidate artifact set and manifest/model-card chain for v2 pipeline | Not a runtime loader itself | Yes |
| `training/nnue/scripts/verify_candidate_v2.py` | Verifies artifact integrity, hash contracts, format detection/runtime-smoke alignment, and required non-default/no-strength statements | Indirectly validates loader acceptance via runtime-smoke fallback path; not the direct runtime loader | Yes |
| `training/nnue/scripts/verified_runtime_load_v2.py` | Enforces verify-then-load contract using verified candidate directory and runtime smoke helper | Yes, via `sirio_nnue_runtime_smoke_contract` after successful verification | Yes |
| `build/sirio_nnue_runtime_smoke_contract` | Stable runtime helper contract that accepts valid SirioNNUE2 candidate path and rejects invalid payloads | Yes, when passed valid verified `candidate.nnue2` | Yes |
| `build/sirio_nnue_internal_activation_contract` (P0-42 single-argument mode) | Internal activation contract checks in single-argument mode with explicit load rejection reporting contract | No (current stable contract path rejects supplied artifacts under this helper mode) | Yes |
| `tests/nnue_candidate_verified_runtime_load_test.py` | End-to-end verified runtime load contract including corruption/tamper/fake rejection gates | Yes, via verified runtime load helper path | Yes |
| `tests/nnue_internal_activation_candidate_v2_test.py` | Internal activation candidate test coverage around verify/runtime-load prechecks and rejection signalling surfaces | Does not establish successful load through internal-activation helper; verifies rejection contract behaviour | Yes |

## 4) Deferred failing paths

### P0-37 (deferred)
- Intended purpose: fixed-FEN candidate corpus report.
- Failed artifact/helper: `evaluate_candidate_fens_v2.py` / `sirio_nnue_eval_corpus_contract`.
- Symptom: `Invalid SirioNNUE2 header contract`.
- Status: reverted/quarantined by **P0-37R**.
- Reason for deferral: helper/runtime load discrepancy not resolved.

### P0-43 (deferred)
- Intended purpose: static default-vs-experimental evaluation comparison.
- Failed artifact/helper: `--compare` mode in `sirio_nnue_internal_activation_contract` and `nnue_static_eval_compare_v2_test.py`.
- Symptom: `Invalid SirioNNUE2 header contract`.
- Status: reverted/quarantined by **P0-43R**.
- Reason for deferral: helper/runtime load discrepancy not resolved.

## 5) Suspected discrepancy surface (non-exhaustive, non-final)
Potential causes are tracked without claiming certainty:
- Wrong path passed to helper.
- Candidate directory passed instead of direct `candidate.nnue2` artifact path.
- Manifest/corpus JSON path opened as network file.
- Argument-order mismatch.
- `cwd` / relative-path mismatch.
- Stale helper binary.
- Helper using stricter/different header validation than stable runtime path.
- Generated candidate modified between verification and load.
- Mismatch between P0-36 runtime-smoke helper path and new helper load path.

## 6) Future diagnostic requirements before retrying P0-37/P0-43
A future local diagnostic must compare and preserve evidence for:
- Absolute `candidate.nnue2` path at every stage.
- SHA-256 at every stage.
- File size at every stage.
- Full `argv` passed to every helper.
- `cwd` of each subprocess.
- Exact loader function used.
- Whether runtime-smoke and failed helper open the same file.
- Whether both helpers use the same `ExperimentalSirioNNUE2Runtime` load path.

## 7) Explicit guardrails
- Do not weaken header/layout validation.
- Do not accept fallback on valid candidate.
- Do not accept fake Stockfish `.nnue` payloads.
- Do not reintroduce P0-37/P0-43 helpers until local path/hash parity is proven.
- Do not merge tests that fail.
- Do not block the rest of the roadmap on deferred helper-report workflows.

## 8) Relationship to roadmap
- P0-37/P0-43 are non-critical reporting/comparison workflows.
- Their deferral does not invalidate P0-36/P0-42.
- The first P0 NNUE replacement track continues from the last stable merge.
- Search, UCI public activation, OpenBench, and Elo validation remain deferred.

## 9) Validation commands (stable suite expected green)
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

This register is a forensic continuity artifact only. It does not restore deferred helpers and does not alter runtime behaviour.
