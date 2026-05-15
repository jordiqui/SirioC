# P0-61 — Capture/NoisyHistory Runtime Update Integration Point Audit / No Behaviour Change

## 1) Scope
This deliverable is **documentation-only** and audits runtime search integration points where CaptureHistory/NoisyHistory updates could be wired in a later behavioural patch.

- No search behaviour change.
- No MovePicker scoring change.
- No negamax/quiescence/LMR/pruning/TT/eval/UCI behaviour change.
- No NNUE runtime behaviour change.
- No Elo/strength claim.

## 2) Current stable state
Current in-tree state before runtime integration:

- **P0-58** read-only CaptureHistory/NoisyHistory MovePicker tactical scoring is present.
- **P0-59** capture/noisy update-policy scaffold is present (`make_capture_noisy_history_update(...)`).
- **P0-60** test-only shadow update harness is present (`CaptureNoisyHistoryUpdateEvent` + apply helper).
- No real runtime capture/noisy history update calls are currently wired in `negamax(...)` or `quiescence(...)`.

## 3) Existing search update inventory (as implemented now)
Inventory documents only existing behaviour:

### 3.1 Quiet history update points
- Location: `src/search.cpp`, `negamax(...)` move loop.
- Trigger:
  - quiet move searched and `score >= beta` => success update,
  - quiet move searched and `score > alpha_original` => success update,
  - otherwise quiet move searched => failure update.
- API used: `context.history.update_quiet_history(mover, move, history_depth, success)`.

### 3.2 Killer update points
- Location: `src/search.cpp`, `negamax(...)` beta-cutoff path.
- Trigger: `alpha >= beta` and cutting move is quiet.
- API used: `context.history.store_killer(move, ply)`.

### 3.3 Beta-cutoff handling
- Main search (`negamax`): cutoff on `alpha >= beta` after move score integration.
- Quiescence (`quiescence`): cutoff on `score >= beta` for tactical move recursion; function returns immediately.

### 3.4 Failed quiet/capture handling visibility
- Quiet failure handling: explicit quiet-history negative update exists in `negamax`.
- Capture/noisy failure handling: no runtime capture/noisy history update path exists.

### 3.5 Quiescence tactical handling
- Only pseudo-legal tactical moves are generated.
- SEE-negative tactical moves are skipped.
- Legal tacticals are searched recursively; beta cutoff returns early; no tactical history write occurs.

### 3.6 TT probe/store relationship around update surfaces
- TT probe occurs before move loop in `negamax`, influencing bounds/TT move ordering.
- TT store occurs after loop using resulting bound type.
- Existing quiet/killer updates happen inside move-loop and before final TT store, but there is no capture/noisy runtime update.

### 3.7 Root handling relevance
- Root uses same `negamax(...)` move-loop/update behaviour at `ply == 0`.
- Root currently has no special tactical history write path separate from generic `negamax` logic.

## 4) Candidate Capture/NoisyHistory runtime update points

### Candidate A — Negamax beta cutoff caused by tactical move
- **File/function/block:** `src/search.cpp`, `negamax(...)`, immediately in/adjacent to `if (alpha >= beta)` block when current move is tactical.
- **Trigger condition:** move score causes fail-high/cutoff (`alpha >= beta`), and current move is non-quiet.
- **Move kind available:** tactical (`capture`, promotion, en-passant, castling-as-noisy per current noisy definition).
- **Depth available:** `history_depth` already computed in scope.
- **Score/cutoff context available:** yes (`score`, `alpha`, `beta`, cutoff state).
- **Key extraction feasibility:** high; board state before undo supports `make_capture_history_key(...)` / `make_noisy_history_key(...)`.
- **Risk level:** **Low-Medium** (localised to one clear cutoff path in main search).
- **Include in first runtime patch?:** **Yes (recommended)**.

### Candidate B — Negamax failed tactical move (non-cutoff)
- **File/function/block:** `src/search.cpp`, `negamax(...)`, post-search move handling where quiet success/failure is currently decided.
- **Trigger condition:** tactical move searched and fails to produce cutoff / insufficient improvement.
- **Move kind available:** tactical.
- **Depth available:** `history_depth` in scope.
- **Score/cutoff context available:** yes.
- **Key extraction feasibility:** high.
- **Risk level:** **Medium** (adds broad negative-feedback surface across many tactical nodes).
- **Include in first runtime patch?:** No.

### Candidate C — Quiescence tactical cutoff
- **File/function/block:** `src/search.cpp`, `quiescence(...)`, `if (score >= beta) return score;`.
- **Trigger condition:** tactical qsearch move causes beta cutoff.
- **Move kind available:** tactical only.
- **Depth available:** qsearch ply available, but not comparable to main-search depth semantics.
- **Score/cutoff context available:** yes.
- **Key extraction feasibility:** medium-high.
- **Risk level:** **Medium-High** (touches volatile qsearch boundary and may reshape tactical horizon behaviour quickly).
- **Include in first runtime patch?:** No.

### Candidate D — Quiescence failed tactical move
- **File/function/block:** `src/search.cpp`, `quiescence(...)`, after recursive score when no cutoff.
- **Trigger condition:** legal tactical searched and `score < beta` (and possibly `score <= alpha` for strict failure).
- **Move kind available:** tactical.
- **Depth available:** qsearch ply only.
- **Score/cutoff context available:** yes.
- **Key extraction feasibility:** medium-high.
- **Risk level:** **High** (very broad update volume and sensitivity in qsearch).
- **Include in first runtime patch?:** No.

### Candidate E — Root tactical move outcome path
- **File/function/block:** `src/search.cpp`, `negamax(...)` with `ply == 0` (shared move loop).
- **Trigger condition:** root tactical move success/failure/cutoff, if explicitly gated to root.
- **Move kind available:** tactical.
- **Depth available:** yes.
- **Score/cutoff context available:** yes.
- **Key extraction feasibility:** high.
- **Risk level:** **Medium** (special-casing root increases policy complexity and asymmetry).
- **Include in first runtime patch?:** No (prefer unified non-root-special-cased path).

## 5) Recommended first runtime update patch (for future P0-62)
Select exactly one integration point:

- **Recommended point:** Candidate A only — update CaptureHistory/NoisyHistory on **negamax tactical beta cutoff** only, excluding quiescence.

Rationale:
- Narrowest behavioural surface among tactical runtime update options.
- Reuses existing cutoff semantics already central to killer updates.
- Avoids qsearch volatility and avoids broad failed-move negative updates in first step.
- Easier to isolate in tests/snapshots/bench baselines because trigger is explicit and rare relative to all tactical traversals.

## 6) Future P0-62 behavioural patch contract
P0-62 must satisfy all items below:

1. Implement exactly **one** runtime update point.
2. Unless explicitly re-approved, do **not** update in qsearch.
3. No LMR/pruning/TT/eval/UCI changes.
4. No MovePicker scoring change beyond P0-58.
5. P0-48 ordering snapshots remain deterministic.
6. Capture/noisy history values change only via the selected new runtime point.
7. Tests must prove capture/noisy update occurs only under expected runtime/simulated condition.
8. No Elo/strength claim from unit/integration tests.

## 7) Rejection conditions for future P0-62
Reject P0-62 if any of the following occurs:

- Updates multiple history families in one patch.
- Changes MovePicker scoring policy.
- Touches qsearch and negamax update integration simultaneously.
- Changes LMR or pruning logic.
- Changes TT store/probe behaviour.
- Changes UCI/evaluation/NNUE behaviour.
- Weakens deterministic snapshot guardrails.
- Adds match/OpenBench/fastchess/ORDO logic.

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

P0-61 is documentation-only. No search behaviour changed. No NNUE behaviour changed. No strength/Elo claim is made.

## 9) P0-62 runtime integration (single-point behavioural patch)
- **Exact integration point:** `src/search.cpp`, `negamax(...)` move loop, inside the `if (alpha >= beta)` cutoff block after score integration.
- **Trigger condition:** main-search cutoff where the cutting move is tactical (`!quiet_move`) and a valid capture/noisy key can be resolved by `make_capture_history_key(...)`/`make_noisy_history_key(...)`.
- **Update target:** one `CaptureNoisyHistoryUpdate` built via `make_capture_noisy_history_update(...)` and applied via `apply_capture_noisy_history_update(...)`.
- **Depth/bonus source:** existing `history_depth` from negamax move-loop flow (same source used in local quiet-history updates).
- **Excluded paths (unchanged):**
  - no qsearch runtime update calls;
  - no failed tactical capture/noisy updates;
  - no quiet cutoff capture/noisy updates;
  - no extra root-only update path.
- **Tests added/extended:** history tests include runtime apply no-op validation for invalid/none target updates; existing capture/noisy policy + shadow-event tests remain active.
- **Limitations:** current coverage is unit-level and deterministic; qsearch exclusion is enforced by search-code inspection and unchanged qsearch implementation (no tactical history write path added there).

## 10) P0-63 observability/test contract for P0-62 runtime update
- Added deterministic, test-only observability for capture/noisy runtime updates through `SearchHistory` counters and a constrained helper API:
  - `CaptureNoisyRuntimeUpdateCounters` (non-production, inspection-only state),
  - `apply_capture_noisy_runtime_update_for_tests(...)` gate helper.
- Observability mechanism confirms the P0-62 trigger contract in one place:
  - applies only when site is `MainNegamaxTacticalBetaCutoff` and update policy resolves a valid capture/noisy target.
- Added deterministic unit coverage for:
  - main tactical beta cutoff applies exactly one capture/noisy runtime update,
  - quiet beta cutoff excluded,
  - qsearch tactical cutoff excluded,
  - failed tactical move excluded,
  - repeated application is deterministic.
- Confirmed excluded paths remain excluded:
  - no qsearch runtime update wiring introduced,
  - no failed tactical runtime update wiring introduced,
  - no additional runtime update point introduced.
- Limitations:
  - this patch provides deterministic contract-level observability via helper+counter tests;
  - full end-to-end node-path orchestration in live negamax/qsearch is still intentionally constrained to avoid brittle position-dependent tests.

## 11) P0-64 note (no capture/noisy runtime-update changes)
- P0-64 adds a read-only ContinuationHistory quiet-ordering hook in MovePicker only.
- No change was made to the existing Capture/NoisyHistory runtime update trigger.
- No new Capture/NoisyHistory runtime update point was added.
- P0-63 observability contract remains valid and unchanged.

## 12) P0-65 note (capture/noisy runtime path unchanged)
- P0-62/P0-63 Capture/NoisyHistory runtime trigger contract is unchanged.
- Newly added ContinuationHistory runtime update is separate and quiet-only.
- No new CaptureHistory runtime update point was added.
- No new NoisyHistory runtime update point was added.

## 13) P0-66 note (capture/noisy runtime path unchanged)
- P0-62/P0-63 Capture/NoisyHistory runtime trigger remains unchanged.
- P0-66 affects only ContinuationHistory quiet beta-cutoff symmetry in main negamax.
- No new CaptureHistory or NoisyHistory runtime update point was added.

## 14) P0-67 note (runtime update path unchanged)
- Existing P0-62/P0-63 Capture/NoisyHistory runtime update path remains unchanged.
- P0-67 adds only read-only Capture/NoisyHistory consumption in MovePicker tactical/noisy scoring.
- No new CaptureHistory runtime update site was introduced.

## 15) P0-103 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-103 scope is ProbCut empty candidate-context observability only.
- No Capture/NoisyHistory scoring or update behavior was changed.

## 15) P0-97 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-97 is ProbCut safety-predicate infrastructure only.
- No Capture/NoisyHistory scoring change was made.
- No Capture/NoisyHistory update-policy/runtime-update change was made.

## 15) P0-92 note (runtime update path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-92 adds ProbCut helper infrastructure only (disabled/no-op under current selectivity flag state).
- No Capture/NoisyHistory scoring or runtime update-location change was made.

## 15) P0-81 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-81 is reverse-futility margin infrastructure only.
- No Capture/NoisyHistory scoring or update behavior changed.
- No new NoisyHistory runtime update site was introduced.
- Runtime observability counter semantics remain unchanged.

## 15) P0-68 note (capture/noisy path unchanged)
- P0-62/P0-63 Capture/NoisyHistory runtime path remains unchanged.
- P0-67 read-only tactical/noisy MovePicker scoring remains unchanged.
- CorrectionHistory P0-68 foundation is storage/API only and does not affect CaptureHistory/NoisyHistory runtime/scoring behavior.

## 16) P0-69 note (capture/noisy path unchanged)
- P0-62/P0-63 Capture/NoisyHistory runtime path remains unchanged.

## 17) P0-94 note (capture/noisy path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-94 is ProbCut threshold/reduction helper infrastructure only.
- No Capture/NoisyHistory scoring change was made.
- No Capture/NoisyHistory runtime update-location change was made.
- P0-67 read-only tactical/noisy MovePicker scoring remains unchanged.
- P0-69 CorrectionHistory read-only static-eval helper work is isolated and does not affect CaptureHistory/NoisyHistory runtime or scoring behavior.

## 17) P0-70 note (capture/noisy path unchanged)
- P0-62/P0-63 Capture/NoisyHistory runtime path remains unchanged.
- P0-67 read-only tactical/noisy MovePicker scoring remains unchanged.
- P0-70 CorrectionHistory runtime key construction is isolated and does not affect Capture/NoisyHistory runtime update or scoring behavior.

## 15) P0-71 note (capture/noisy runtime and read-only tactical scoring unchanged)
- P0-62/P0-63 Capture/NoisyHistory runtime update path remains unchanged.
- P0-67 read-only Capture/NoisyHistory MovePicker tactical scoring remains unchanged.
- P0-71 CorrectionHistory read-only static-eval wiring in main negamax does not alter Capture/NoisyHistory runtime contracts.

## P0-72 note
- P0-62/P0-63 Capture/NoisyHistory runtime update path remains unchanged.
- P0-67 read-only tactical/noisy MovePicker scoring remains unchanged.
- CorrectionHistory runtime update in P0-72 is separate and quiet-only.

## P0-73 note
- No CaptureHistory/NoisyHistory runtime behaviour changed.
- This step is CorrectionHistory helper naming/API boundary cleanup only.

## 15) P0-74 note (Capture/Noisy separation unchanged)
- P0-62/P0-63 Capture/NoisyHistory runtime path remains unchanged.
- P0-67 read-only Capture/Noisy tactical scoring remains unchanged.
- P0-74 CorrectionHistory fail-low negative update is separate and does not affect Capture/NoisyHistory runtime/scoring.

## 15) P0-75 note
- Capture/NoisyHistory runtime path remains unchanged.
- P0-75 affects only CorrectionHistory runtime update magnitude hardening (scaled/clamped delta policy).
- No Capture/NoisyHistory scoring or runtime update behavior changed.

## 15) P0-76 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime update path remains unchanged from P0-62/P0-63.
- P0-76 adds search-selectivity parameter infrastructure only.
- No CaptureHistory scoring change.
- No NoisyHistory scoring change.
- No Capture/Noisy runtime update policy change.

## 16) P0-77 note
- Capture/NoisyHistory runtime path remains unchanged.
- P0-77 adds reverse-futility helper infrastructure only.
- No CaptureHistory/NoisyHistory scoring or runtime update behavior changed.

## 18) P0-78 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-78 is limited to reverse-futility disabled probe wiring in main negamax only.
- No CaptureHistory or NoisyHistory scoring/update policy change was made.

## 15) P0-79 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-79 scope is reverse-futility disabled return scaffolding only.
- No Capture/NoisyHistory scoring or update behaviour changed.

## 15) P0-80 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-80 is reverse-futility safety-predicate infrastructure only.
- No Capture/NoisyHistory scoring or runtime update policy changed.

## 15) P0-82 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-82 is reverse-futility guarded-return observability only.
- No Capture/NoisyHistory scoring or update behaviour changed.

## P0-83 note
- Capture/NoisyHistory runtime path remains unchanged.
- P0-83 affects only reverse futility selectivity activation under existing conservative guards.
- No Capture/NoisyHistory scoring or update behavior was changed.

## P0-84 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-84 is move-count-pruning helper infrastructure only.
- No Capture/NoisyHistory scoring or update behavior changed.

## 18) P0-85 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-85 is Move Count Pruning disabled probe wiring only.
- No Capture/NoisyHistory scoring or update behavior changed.


## 15) P0-86 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-86 is Move Count Pruning disabled guarded-continue scaffolding only.
- No Capture/NoisyHistory scoring or runtime update behavior changed.

## 15) P0-87 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-87 adds Move Count Pruning guarded-continue observability only.
- No Capture/NoisyHistory scoring or runtime update behavior changed.

## P0-88 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-88 is MCP move-safety predicate infrastructure only.
- No Capture/NoisyHistory scoring or runtime update behavior changed.

## P0-89 note
- Capture/NoisyHistory runtime path remains unchanged.
- P0-89 is Move Count Pruning threshold infrastructure only.
- No Capture/NoisyHistory scoring or update behavior changed.

## 15) P0-90 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-90 is MCP threshold-constant preparation only.
- No Capture/NoisyHistory scoring or update behavior changed.

## P0-91 note
- Capture/NoisyHistory runtime path remains unchanged.
- P0-91 affects only Move Count Pruning selectivity activation.
- No Capture/NoisyHistory scoring or update behavior changed.

## 18) P0-93 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-93 is ProbCut disabled probe wiring only.
- No Capture/NoisyHistory scoring or update change.

## 15) P0-95 note (capture/noisy runtime unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-95 adds ProbCut probe observability only.
- No Capture/NoisyHistory scoring or update behavior changed.

## P0-96 note (capture/noisy runtime unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-96 is ProbCut guarded parameter scaffolding only.
- No Capture/NoisyHistory scoring or update behavior changed.


## P0-98 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime update path remains unchanged.
- P0-98 introduces ProbCut candidate-context infrastructure only.
- No Capture/NoisyHistory scoring or runtime update policy changed.

## P0-99 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-99 is ProbCut explicit-property candidate classification infrastructure only.
- No Capture/NoisyHistory scoring or runtime update behavior changed.

## 15) P0-100 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-100 is ProbCut candidate-selection interface infrastructure only.
- No Capture/NoisyHistory scoring or update behaviour changed.

## 15) P0-101 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-101 adds ProbCut explicit-flags selector infrastructure only.
- No Capture/NoisyHistory scoring or runtime update behavior changed.

## 18) P0-102 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-102 is ProbCut explicit false-flags runtime selector wiring only.
- No Capture/NoisyHistory scoring change was made.
- No Capture/NoisyHistory runtime update change was made.

## 22) P0-104 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime update path remains unchanged.
- P0-104 adds ProbCut cutoff-decision helper infrastructure only.
- No CaptureHistory or NoisyHistory scoring/update behavior was changed.

## P0-105 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime update path remains unchanged.
- P0-105 is ProbCut reduced-search result context infrastructure only.
- No Capture/NoisyHistory scoring or runtime update behavior changed.

## P0-106 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime update path remains unchanged.
- P0-106 adds only an empty ProbCut reduced-result runtime scaffold in guarded main-negamax code.
- Runtime reduced-search result remains empty/no-result.
- `should_cutoff_probcut(...)` remains unwired from runtime.
- No Capture/NoisyHistory scoring or runtime update behavior changed.

## P0-107 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime update path remains unchanged.
- P0-107 wires `should_cutoff_probcut(...)` only to an empty/no-result reduced-search context under guarded main negamax ProbCut block.
- No return/cutoff/search/reduced-depth search behavior was added.
- `selectivity_probcut_enabled` remains `false` and qsearch remains clean.
- No Capture/NoisyHistory scoring or runtime update behavior changed.

## P0-108 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime update path remains unchanged.
- P0-108 adds ProbCut cutoff-decision observability only, guarded by `if (probcut_cutoff)` in main negamax.
- Runtime ProbCut candidate context remains empty/no-candidate.
- Runtime ProbCut reduced-search result remains empty/no-result.
- No Capture/NoisyHistory scoring or runtime update behavior changed.

## P0-109 note (capture/noisy runtime path unchanged)
- P0-109 adds only a guarded future ProbCut return scaffold in main negamax under `if (probcut_cutoff)`.
- The existing guarded observability call `record_probcut_cutoff_decision()` is preserved and the block now includes `return probcut_result.value;`.
- Return path remains unreachable under defaults (`selectivity_probcut_enabled = false`; runtime reduced-search result remains empty/no-result).
- Runtime ProbCut candidate context remains empty/no-candidate.
- No ProbCut search or reduced-depth search execution was added.
- qsearch remains clean of ProbCut helper/probe/cutoff-decision wiring.
- Capture/NoisyHistory behavior remains unchanged.

## P0-110 note (capture/noisy runtime path unchanged)
- P0-110 adds only a deterministic ProbCut reduced-search request context foundation in `search_params`.
- Main negamax runtime request remains explicit empty/no-request and discarded.
- No reduced-depth search, no ProbCut candidate selection, and no capture/noisy runtime wiring changes were introduced.
- Capture/NoisyHistory behaviour remains unchanged.

## P0-111 ProbCut Empty Reduced-Search Request Observability / No Runtime Invocation Contract
- Added deterministic SearchHistory observability for empty ProbCut reduced-search request state via `record_probcut_empty_reduced_search_request()` and test counter accessor.
- Main negamax guarded ProbCut block now records empty request only when `!probcut_request.has_request`.
- Runtime ProbCut request remains explicit empty/no-request (`empty_probcut_reduced_search_request()`).
- No non-empty request, no candidate selection, and no reduced-depth search invocation were added.
- `selectivity_probcut_enabled` remains `false`; P0-109 guarded return scaffold remains unreachable under defaults.
- qsearch remains clean of ProbCut helper/probe/request/cutoff wiring.
- Reverse futility (P0-83), MCP (P0-91), CorrectionHistory, Capture/NoisyHistory, ContinuationHistory, TT, MovePicker, UCI, and NNUE remain unchanged.

## 16) P0-112 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-112 adds ProbCut reduced-search request-builder infrastructure only.
- No Capture/NoisyHistory scoring or runtime update behavior was changed.

## P0-113 note
- Capture/NoisyHistory runtime path remains unchanged.
- P0-113 scope is ProbCut request-builder empty runtime wiring only.
- No Capture/NoisyHistory scoring or update behavior changed.

## 31) P0-114 note (capture/noisy runtime path unchanged)
- CaptureHistory/NoisyHistory runtime path remains unchanged.
- P0-114 scope is ProbCut reduced-search request-builder candidate-gate wiring only.
- No CaptureHistory/NoisyHistory scoring or runtime update policy changed.

## P0-115 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-115 is ProbCut candidate-placeholder variable wiring only.
- No Capture/NoisyHistory scoring or update change.

## P0-116 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime update path remains unchanged.
- P0-116 introduces ProbCut candidate-source authority infrastructure only.
- No CaptureHistory/NoisyHistory scoring or runtime update behavior changed.

## 35) P0-117 note
- Capture/NoisyHistory runtime path remains unchanged.
- P0-117 adds ProbCut candidate-source observability only.
- No Capture/NoisyHistory scoring or update behavior was changed.

## 15) P0-118 note (capture/noisy runtime path unchanged)
- CaptureHistory/NoisyHistory runtime update path remains unchanged.
- P0-118 transitions ProbCut runtime source to `ExplicitFlags` while keeping all explicit flags false.
- No CaptureHistory/NoisyHistory scoring or runtime update behaviour was changed.

## P0-119 note
- Capture/NoisyHistory runtime path remains unchanged.
- P0-119 scope is ProbCut `ExplicitFlags` source observability only.
- No Capture/NoisyHistory scoring change and no Capture/NoisyHistory runtime update-policy change.

## P0-120 note
- Capture/NoisyHistory runtime path remains unchanged.
- P0-120 adds ProbCut candidate-eligibility gating infrastructure only.
- No Capture/NoisyHistory scoring or runtime update policy change was made.

## P0-121 note
- Capture/NoisyHistory runtime path remains unchanged.
- P0-121 adds ProbCut ineligible-candidate observability only.
- No Capture/NoisyHistory scoring or runtime update behavior changed.

## 15) P0-122 note (capture/noisy runtime path unchanged)
- CaptureHistory/NoisyHistory runtime update path remains unchanged.
- P0-122 adds ProbCut candidate-flags context infrastructure only.
- No CaptureHistory/NoisyHistory scoring or runtime update behavior changed.


## P0-123 note
- Capture/NoisyHistory runtime path remains unchanged.
- P0-123 is ProbCut empty candidate-flags observability only.
- No Capture/NoisyHistory scoring or update change.

## 18) P0-124 note (capture/noisy runtime path unchanged)
- Capture/NoisyHistory runtime path remains unchanged.
- P0-124 is ProbCut candidate-flags helper infrastructure only.
- No Capture/NoisyHistory scoring or update behavior changed.

## P0-125 note
- Capture/NoisyHistory runtime path remains unchanged.
- P0-125 is ProbCut candidate-flags non-empty probe infrastructure only.
- No Capture/NoisyHistory scoring or update change.

## P0-126 note
- Capture/NoisyHistory runtime path remains unchanged.
- P0-126 scope is ProbCut non-empty candidate-flags probe observability only.
- No Capture/NoisyHistory scoring or update behavior changed.
