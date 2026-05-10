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
