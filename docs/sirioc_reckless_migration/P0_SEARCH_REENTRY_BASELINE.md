# P0-46 — Search P0 Re-Entry Baseline / No-Behaviour-Change Guardrail

## 1) Scope
This deliverable is **documentation-only** and establishes a search P0 re-entry baseline.

- No behaviour change.
- No Elo/strength claim.
- No NNUE activation.
- No match/gauntlet work.
- No search logic edits in this patch.

## 2) Current stable entry point
- P0-45 evaluation-track readiness gate is closed and documents the stable NNUE track boundary.
- P0-37 and P0-43 remain deferred.
- SirioNNUE2 remains non-default.
- Search P0 re-entry starts from current stable search behaviour already present in tree.

## 3) Current search architecture inventory (observed in-tree)
Only items observed in current code are listed:

- Iterative deepening root loop is present (`run_search_thread` depth loop).
- Aspiration windows are present and widened on fail-low/fail-high.
- Negamax search with PVS-like null-window re-search path is present.
- Quiescence search is present and called at depth exhaustion.
- Null move pruning is present (guarded by depth/static eval/non-pawn material checks).
- Futility pruning is present at shallow depth (`futility_margin_depth1`).
- LMR is present via precomputed reduction table + runtime adjustments.
- Killer moves are present (two slots per ply).
- Quiet history heuristic is present (`quiet_history_[color][from][to]`).
- SEE/static exchange scoring exists (`static_exchange_score`) and is used in move ordering.
- `MovePicker` exists and orders using TT move, tacticals, killers, and quiet-history ordering.
- TT move ordering is present via TT probe + TT move handoff to `MovePicker`.
- Root result publication is present through UCI `info`/`bestmove` output and final `SearchResult` aggregation.
- Time checks are present through shared time limits, periodic node-based checks, and timeout flags.
- SMP touchpoints are present: configurable thread count, worker coordination, shared stop/time/node state, per-thread local results, and primary-thread publication with staggered starts (lazy-SMP style coordination touchpoints).

## 4) Existing extracted modules
### `include/sirio/search_params.hpp`
Status: extracted central constants module for search/runtime tuning constants (mate bounds, LMR table sizes, history limits, futility margin, node/time check intervals, max threads).

### `include/sirio/history.hpp` + `src/history.cpp`
Status: extracted search-history module.

Current in-tree history functionality:
- Quiet history score read/update.
- Killer move storage and retrieval.
- Quiet-move classifier helper.

Currently absent in-tree (not implemented in this module today):
- Continuation history.
- Noisy history.
- Correction history.
- Capture history.
- Counter-move history.

## 5) Risk map for future planned search changes
- MovePicker scoring changes: **High risk** (ordering ripple across all nodes; unstable tactical/quiet balance can shift pruning behaviour indirectly).
- Quiet/noisy history extension: **High risk** once wired into ordering (feedback loops can alter node topology quickly).
- Correction history: **Medium-High risk** (eval-dependent bias can change beta-cut structure and TT reuse patterns).
- ProbCut: **High risk** (new tactical cut mechanism can create tactical blindness if thresholds are wrong).
- Singular extensions: **High risk** (depth inflation around TT moves; sensitive to TT quality/flags).
- Reverse futility: **Medium risk** (selective fail-low pruning can under-search strategic defenses).
- Move count pruning: **Medium-High risk** (late-move culling highly position/time-control sensitive).
- Qsearch SEE pruning: **High risk** (qsearch tactical horizon sensitivity; easy to miss forcing sequences).
- Root PV stability/time allocation heuristics: **High risk** (can degrade practical strength even when node count rises).
- TT replacement/PV flag interaction: **High risk** (wrong replacement or bound typing can corrupt PV and cutoff quality).

## 6) Proposed safe search surgery order (planning only)
No implementation in this patch. Recommended controlled order:

1. **P0-47**: MovePicker scoring audit / no behaviour change.
2. **P0-48**: noisy/capture-history data-structure scaffold / no search use.
3. **P0-49**: continuation-history data-structure scaffold / no search use.
4. **P0-50**: correction-history scaffold / no search use.
5. Later: controlled MovePicker integration (one behavioural change per patch).
6. Later: ProbCut.
7. Later: singular extensions.
8. Later: qsearch SEE/TT-aware selectivity changes.
9. Later: PV stability/time heuristics.

## 7) Guardrails
- Maximum one behavioural search change per patch.
- Every behavioural patch must include baseline-vs-candidate tests/bench evidence.
- No simultaneous NNUE and search behavioural changes.
- No fastchess/OpenBench until explicitly approved.
- No Elo claims from unit/integration tests.
- No copying code from Reckless/Stockfish/Berserk.
- Inspiration-only policy: implementation must be original and written inside SirioC.

## 8) Validation suite (stable command list)
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

P0-46 is documentation-only. Search behaviour is unchanged. NNUE behaviour is unchanged. SirioNNUE2 remains non-default. No strength/Elo claim is made.
