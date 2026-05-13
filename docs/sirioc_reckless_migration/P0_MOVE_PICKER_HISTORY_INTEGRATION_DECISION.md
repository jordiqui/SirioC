# P0-57 — MovePicker History Integration Decision Matrix / No-Behaviour-Change Contract

## 1) Scope
This deliverable is **documentation-only** and records the first controlled history-integration decision point before any behavioural MovePicker/search patch.

- No search behaviour change.
- No MovePicker scoring change.
- No new history reads from MovePicker.
- No new history writes from search.
- No Elo/strength claim.

## 2) Current readiness state
From the current in-tree docs/tests and search/history implementation baseline:

- **P0-48** MovePicker ordering snapshots exist and enforce deterministic ordering/legality/duplicate-move guardrails.
- **P0-49** capture-history and noisy-history scaffolds exist in `SearchHistory` with isolated lifecycle tests.
- **P0-50** continuation-history scaffold exists with isolated lifecycle tests.
- **P0-51** correction-history scaffold exists with isolated lifecycle tests.
- **P0-52** lifecycle audit exists.
- **P0-53 / P0-54 / P0-55** key extraction contracts exist.
- **P0-56** aggregate key-readiness audit exists.

Implementation readiness signals observed:
- MovePicker currently reads only TT move, killers, quiet history, capture/promotions partitioning, and SEE/MVV-LVA priorities.
- Capture/noisy/continuation/correction tables are present and independently testable, but are not yet integrated into MovePicker/search behaviour.
- Existing P0-48 snapshot harness provides a direct no-regression boundary for first ordering integration.

## 3) Candidate first integration options

### Option A — Capture/NoisyHistory read-only scoring in MovePicker
- **Expected benefit:** Adds a direct tactical ordering signal on capture/noisy classes where MovePicker already has staged tactical partitions; likely highest immediate ordering leverage while remaining local to picker logic.
- **Risk:** Medium. Any tactical reordering can alter cutoff topology; must preserve TT-first and legal/duplicate invariants.
- **Dependency:** P0-49 scaffold + P0-48 snapshot harness (already present).
- **Affected code (future behavioural patch):** MovePicker scoring path in `src/search.cpp`; snapshot expectations/tests in `tests/move_picker_snapshot_tests.cpp`.
- **Required tests:** Snapshot before/after updates, deterministic ordering checks, legal-move-set invariants, no-duplicates assertions, TT-priority assertions, full regression suite.
- **Changes move ordering:** Yes.
- **Requires search updates:** No (read-only integration can be done without adding search writes in first behavioural step).

### Option B — ContinuationHistory read-only scoring in MovePicker
- **Expected benefit:** Adds path-dependent quiet/tactical ordering context beyond local move features.
- **Risk:** Medium-High. Depends on previous-move context and can be brittle at root/transposition boundaries; higher risk of hidden coupling to caller context.
- **Dependency:** P0-50 scaffold + robust previous-move plumbing audit.
- **Affected code (future behavioural patch):** MovePicker + call-site context wiring in `negamax` stack.
- **Required tests:** All Option A tests plus explicit previous-move-context consistency checks across ply/root.
- **Changes move ordering:** Yes.
- **Requires search updates:** Likely yes (context plumbing and/or updates for meaningful signal).

### Option C — CorrectionHistory use in eval/search
- **Expected benefit:** Potential score correction that may reduce systematic eval bias.
- **Risk:** High for first integration. Directly touches score path/bounds and can perturb TT store/probe behaviour and pruning decisions.
- **Dependency:** P0-51 scaffold + explicit eval/search contract work.
- **Affected code (future behavioural patch):** `negamax`/evaluation pathway, possibly TT interaction surfaces.
- **Required tests:** Score-path invariants, TT bound stability checks, plus full ordering/search regression.
- **Changes move ordering:** Indirectly yes (through altered node scores/cutoffs).
- **Requires search updates:** Yes.

### Option D — Quiet history retuning
- **Expected benefit:** May rebalance quiet ordering quality in existing heuristic.
- **Risk:** Medium-High. Behavioural retuning without new signal risks conflating baseline with tuning effects; harder forensic attribution.
- **Dependency:** none beyond existing quiet history.
- **Affected code (future behavioural patch):** quiet scoring constants/weighting and possibly update policy.
- **Required tests:** Snapshot deltas + broader search bench comparisons to separate noise from intent.
- **Changes move ordering:** Yes.
- **Requires search updates:** Potentially (if update policy/bonuses changed).

### Option E — Killer/history weighting adjustment
- **Expected benefit:** Could rebalance quiet-stage prioritization and cutoff timing.
- **Risk:** Medium-High. Cross-couples two existing signals and may silently alter LMR application distribution via move-index shifts.
- **Dependency:** Existing killer + quiet history behaviour.
- **Affected code (future behavioural patch):** quiet/killer stage precedence/weighting in MovePicker and related ordering logic.
- **Required tests:** Snapshot deltas, TT-priority preservation, move-index/LMR interaction audit, full regression suite.
- **Changes move ordering:** Yes.
- **Requires search updates:** Not strictly, but may require follow-up tuning if side effects emerge.

## 4) Recommended first integration
**Recommended first candidate: Option A — Capture/NoisyHistory read-only scoring in MovePicker only.**

Rationale:
- It is the most localized behavioural surface (ordering only, no score-path mutation).
- It aligns with the P0-48 snapshot harness, enabling precise before/after deterministic assertions.
- It avoids first-step coupling to negamax/qsearch/LMR/TT/eval contracts.
- It supports strict forensic attribution: one new signal, one ordering surface, one guardrail bundle.

## 5) Future behavioural patch contract (P0-58)
The first behavioural integration patch must satisfy all of the following:

1. Integrate **one signal only** (capture/noisy read-only ordering signal).
2. No simultaneous LMR/pruning/selectivity changes.
3. No search update writes to new history tables unless explicitly and separately authorized.
4. Maintain and update exact MovePicker snapshot coverage (before/after expectations where required).
5. Preserve legal move invariants (all emitted moves legal).
6. Preserve no-duplicate invariant.
7. Preserve TT move priority semantics.
8. Preserve deterministic ordering for identical inputs.
9. Keep evaluation/search/UCI behaviour unchanged except intended ordering effects.
10. No Elo/strength claim from unit or deterministic tests.

## 6) Rejection conditions
The future behavioural integration patch must be rejected if any of the following occur:

- TT move priority changes unexpectedly.
- Generated legal move set changes (beyond ordering).
- Duplicate moves are introduced.
- Multiple new signals are combined in one patch.
- LMR/pruning/TT/eval/UCI behaviour is changed.
- P0-48 snapshot harness is weakened or bypassed.
- Existing guardrails are removed without replacement.

## 7) Validation suite (stable command list)
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

P0-57 is documentation-only. No search behaviour is changed in this patch. No NNUE runtime behaviour is changed in this patch. No strength/Elo claim is made.

## 8) P0-58 implementation update
- CaptureHistory/NoisyHistory read-only scoring is now integrated in `MovePicker` for tactical candidates.
- One-signal-only behavioural patch confirmation: only capture/noisy history signal was added to MovePicker scoring.
- No search update path was added (no negamax/quiescence/beta-cutoff history updates for these tables in this patch).
- Tests were extended to validate:
  - strict zero-history snapshot equivalence continuity,
  - TT move priority preservation with non-zero history,
  - bounded reordering effects for non-zero capture/noisy history.
- Snapshot impact: baseline P0-48 snapshots remain unchanged under default/zero history state.
- Deferred work remains unchanged: search-side history updates, continuation history integration, correction history integration, and any LMR/pruning changes.

## 9) P0-59 update-policy scaffold (capture/noisy search-side contract prep)
- Added isolated capture/noisy update-policy helper contract:
  - `CaptureNoisyHistoryUpdate`
  - `make_capture_noisy_history_update(...)`
  - `apply_capture_noisy_history_update_for_tests(...)` (test-only adapter)
- Scope remains scaffold-only:
  - no `negamax` runtime wiring,
  - no `quiescence` runtime wiring,
  - no MovePicker scoring changes beyond P0-58.
- Policy behaviour captured in tests:
  - capture success/failure decision routing,
  - noisy-promotion success routing,
  - quiet/invalid-key rejection,
  - bounded bonus/clamp preservation,
  - deterministic repeated update behaviour.
- Future integration of real search update calls remains explicitly deferred.

## 10) P0-60 search update shadow harness (no runtime integration)
- Added test-only capture/noisy shadow search-update event harness:
  - `CaptureNoisyHistoryUpdateEvent`
  - `make_capture_noisy_history_update_event_for_tests(...)`
  - `apply_capture_noisy_history_update_event_for_tests(...)`
- Event fields are explicit and deterministic:
  - update target (`None` / `Capture` / `Noisy`),
  - optional capture key,
  - optional noisy key,
  - depth,
  - success/failure sign,
  - optional reason/status string.
- Harness application contract:
  - event helper resolves updates via `make_capture_noisy_history_update(...)`,
  - update effects are applied only through `apply_capture_noisy_history_update_for_tests(...)`,
  - no live search-node dependency,
  - no global mutable-state dependency.
- Added shadow-harness tests for:
  - capture success/failure update direction,
  - noisy promotion success updates,
  - quiet/non-target and invalid-event no-op behavior,
  - deterministic repeated sequence behavior,
  - reset/clear removing event-applied updates,
  - no runtime negamax/qsearch wiring.
- Explicitly deferred: any real negamax/qsearch integration of capture/noisy updates.

## 11) P0-61 runtime update integration-point audit (no runtime wiring yet)
- Added `docs/sirioc_reckless_migration/P0_CAPTURE_NOISY_RUNTIME_UPDATE_AUDIT.md` to audit exact runtime search integration points for future capture/noisy history updates.
- Audit confirms current state remains:
  - P0-58 read-only MovePicker tactical scoring,
  - P0-59 update-policy scaffold,
  - P0-60 test-only shadow harness,
  - no real runtime negamax/qsearch capture/noisy updates.
- Recommended conservative first behavioural runtime step for future P0-62:
  - **one integration point only**: update Capture/NoisyHistory on **negamax tactical beta cutoff**,
  - exclude qsearch in first step,
  - preserve deterministic snapshot guardrails and avoid LMR/pruning/TT/eval/UCI changes.

## 12) P0-62 runtime capture/noisy update integration
- First runtime capture/noisy search update is now wired at one point only: negamax tactical beta-cutoff in main search.
- Signal scope remains intentionally narrow: one success signal only, no failed tactical updates.
- No qsearch integration was added.
- No LMR/pruning/selectivity policy changes were made.
- No TT probe/store policy changes were made.
- No MovePicker scoring changes were made in this step (P0-58 scoring remains as-is).

## 13) P0-63 observability checkpoint
- P0-62 runtime capture/noisy update point is now deterministic and test-observable through history-level counters and a constrained test helper.
- Coverage explicitly validates that only main-negamax tactical beta-cutoff is allowed to apply this runtime update contract.
- No new MovePicker behaviour was added.
- No new search behaviour was added beyond observability of the existing P0-62 single update point.

## 14) P0-64 continuation-history read-only quiet scoring hook
- ContinuationHistory is now connected to MovePicker as a **read-only** quiet-move scoring contribution.
- Integration is intentionally constrained to the existing quiet scoring path only; tactical capture/promotion/noisy ordering paths are unchanged.
- Zero-state ordering contract is preserved: when ContinuationHistory is zero-initialized, MovePicker ordering remains identical to established P0-48/P0-58 baseline snapshots.
- Previous-move context gating is strict:
  - if previous context is unavailable (missing previous board/move),
  - or key extraction is invalid/not safely representable by existing key contracts,
  - contribution is zero (no fallback semantics are invented).
- No ContinuationHistory runtime update point exists yet in search.
- P0-62/P0-63 Capture/NoisyHistory runtime-update path is unchanged.
- No qsearch/LMR/pruning/TT/eval/UCI contract changes were introduced in this step.

## 15) P0-65 continuation-history runtime update (single-point quiet beta-cutoff)
- ContinuationHistory is now both:
  - read by MovePicker quiet scoring (P0-64), and
  - updated at exactly one runtime point.
- Exact runtime update point is constrained to **main negamax quiet beta cutoff only**.
- No qsearch ContinuationHistory update was added.
- No capture/noisy/promotion ContinuationHistory update was added.
- No failed quiet ContinuationHistory malus path was added.
- Previous-context gating remains strict:
  - requires previous board + previous move availability,
  - requires successful continuation-key extraction.
- If context/key is missing or invalid, no ContinuationHistory write occurs.
- Zero-state and deterministic MovePicker snapshot contracts remain protected.

## 15) P0-66 quiet beta-cutoff continuation symmetry
- ContinuationHistory runtime update at main-negamax quiet beta cutoff now applies narrow symmetry.
- The quiet cutoff move keeps the existing positive update.
- Previously searched quiet moves from the same node now receive a conservative malus.

## 16) P0-97 ProbCut move-safety predicate expansion (still disabled / no-search)
- ProbCut helper safety predicate is expanded with explicit candidate-move gates:
  - `has_candidate_move`
  - `is_candidate_capture_or_noisy`
  - `is_candidate_promotion`
- Current node-level probe in main negamax passes safe placeholders (`false, false, false`) and remains a no-op.
- `selectivity_probcut_enabled` remains `false`.
- P0-96 guarded parameter scaffold remains unchanged and unreachable under current defaults.
- No ProbCut search, reduced-depth search, return, or cutoff was added.
- Future candidate-move selection for ProbCut remains explicitly deferred.
- Reverse futility (P0-83) remains unchanged.
- Move Count Pruning (P0-91) remains unchanged.
- Singular extensions remain disabled.
- No LMR, qsearch, MovePicker, TT, UCI, or NNUE behaviour changed in this step.
- Updates are strictly gated by valid previous board+move context and valid continuation key extraction; no fallback semantics are introduced.
- No updates were added for qsearch, capture/noisy, promotion, failed/skipped/illegal paths.
- Zero-state MovePicker ordering contract remains protected.

## 16) P0-67 capture/noisy read-only tactical scoring consolidation
- CaptureHistory/NoisyHistory consumption is now centralized via a read-only tactical MovePicker scoring helper.
- The tactical/noisy contribution is applied only in tactical MovePicker paths (captures/en-passant/promotions).
- Zero-state tactical/noisy ordering remains unchanged under deterministic snapshot coverage.
- No new CaptureHistory/NoisyHistory runtime update point was introduced.
- P0-62/P0-63 capture/noisy runtime update contract is unchanged.
- P0-64/P0-65/P0-66 ContinuationHistory read/update contracts remain unchanged.
- No qsearch, LMR, pruning, TT, eval, UCI, or NNUE runtime behavior changes were introduced.

## 17) P0-68 correction-history zero-runtime foundation
- CorrectionHistory foundation was added/standardized in the history layer as storage/API only.
- SearchHistory now owns and clears CorrectionHistory storage as part of normal history lifecycle.
- No MovePicker consumption was added.
- No search consumption was added.
- No eval/pruning/LMR/TT/qsearch/UCI/NNUE behavior was changed.
- This step is a deterministic zero-runtime foundation for a later, separately authorized correction-history integration patch.

## 18) P0-94 ProbCut threshold/reduction helper foundation (disabled / no-op)
- Added centralized ProbCut helper contracts in `search_params`:
  - beta-threshold helper (`beta + probcut_margin`),
  - reduced-depth helper (`max(0, depth - probcut_reduction)`).
- `selectivity_probcut_enabled` remains `false`.
- P0-93 main-negamax ProbCut probe remains no-op wiring only.
- No ProbCut search was added.
- No ProbCut reduced-depth search was added.
- No ProbCut return/cutoff was added.
- Reverse futility (P0-83) remains unchanged.
- Move Count Pruning (P0-91) remains unchanged.
- Singular extensions remain disabled.
- No LMR, qsearch, MovePicker, TT, UCI, or NNUE behavior changed.

## 18) P0-92 ProbCut helper foundation (disabled / no-behaviour contract)
- Added centralized ProbCut helper foundation in search parameters only (constants + decision helper contract).
- `selectivity_probcut_enabled` remains `false`, so helper resolution is disabled/no-op under default conditions.
- No ProbCut runtime search was added.
- No ProbCut return was added.
- No ProbCut reduced-depth search was added.
- Reverse futility (P0-83) remains enabled and unchanged.
- Move Count Pruning (P0-91) remains enabled and unchanged.
- Singular extensions remain disabled.
- No LMR, qsearch, MovePicker, TT, UCI, or NNUE behaviour changed.

## 18) P0-81 reverse futility margin helper (disabled/no-behavior-change)
- Added a centralized reverse futility margin helper in `search_params` for deterministic margin computation.
- Margin calculation is centralized, deterministic, and bounded to non-negative output.
- `selectivity_reverse_futility_enabled` remains `false`.
- P0-79 guarded reverse-futility return scaffold remains unreachable under defaults.
- No behavior change.
- No move count pruning/probcut/singular extension wiring changes.
- No LMR, qsearch, MovePicker, TT, UCI, or NNUE behavior changes.

## 18) P0-69 correction-history read-only static-eval helper contract
- Added a read-only CorrectionHistory static-eval correction helper in the history layer.
- Runtime wiring into main negamax static eval is deferred in this patch because P0-68 exposes only deterministic placeholder/test key construction (`Color + bucket`) and does not yet provide a safe board-derived runtime key contract.
- Helper behavior is deterministic and read-only:
  - returns raw static eval when key is unavailable/invalid;
  - returns raw static eval when CorrectionHistory score is zero;
  - returns raw static eval plus correction delta when seeded in tests.
- No CorrectionHistory runtime update point was added.
- No MovePicker consumption was added.
- No pruning/LMR/probcut/singular/qsearch/TT/UCI/NNUE behavior changed.
- Zero-state behavior is preserved by construction.

## 18) P0-70 correction-history runtime key contract foundation (no consumption)
- Added a read-only, deterministic runtime key-construction helper: `make_correction_history_key_from_position(const Board&)`.
- Contract inputs are strictly board-derived and stable: side-to-move plus pawn-occupancy-derived deterministic bucket.
- The helper returns `std::optional<CorrectionHistoryKey>` and preserves P0-68 key validity gating.
- No CorrectionHistory runtime update point was added.
- No static-eval runtime hook in search/eval was added.
- No MovePicker/pruning/LMR/qsearch/TT/UCI/NNUE runtime behavior was changed.

## 16) P0-71 correction-history read-only main-negamax static-eval wiring
- CorrectionHistory is now consumed **read-only** in main `negamax` static-evaluation flow.
- The board-derived key helper used is `make_correction_history_key_from_position(const Board&)` (P0-70 contract).
- The correction helper used is `apply_correction_history_to_static_eval(...)` (P0-69 contract).
- Zero-state contract remains preserved: with default/cleared history, corrected static eval equals raw static eval.
- No CorrectionHistory runtime update point was added.
- qsearch remains unchanged and does not consume CorrectionHistory.
- No direct MovePicker/pruning/LMR/probcut/singular/TT/UCI/NNUE behavioural wiring was added in this step.

## 15) P0-72 correction history quiet beta-cutoff runtime update
- CorrectionHistory now has exactly one runtime update point in main negamax quiet beta-cutoff handling.
- Update is positive-only and uses `raw_static_eval` as the learning baseline (`beta - raw_static_eval` gate via helper).
- No qsearch/capture/noisy/promotion/non-cutoff runtime correction updates were added.
- P0-71 read-only correction application path remains in place.
- No MovePicker/pruning/LMR/probcut/singular/TT/UCI/NNUE behavior was directly changed.

## 18) P0-73 correction-history runtime helper API boundary cleanup
- CorrectionHistory quiet beta-cutoff runtime helper naming/API boundary was cleaned.
- Production search now calls `apply_correction_history_quiet_beta_cutoff_update(...)`.
- `*_for_tests` naming is retained only as a test-facing wrapper that forwards to the production helper.
- No behaviour change was introduced.
- P0-72 correction update contract (gating and delta semantics) is unchanged.

## 15) P0-74 note (CorrectionHistory fail-low negative counterpart)
- Added a CorrectionHistory negative runtime update counterpart for main negamax fail-low nodes.
- Update is main negamax fail-low only and requires a valid P0-70 correction key.
- Update is negative-only and uses `raw_static_eval` baseline (`best_value - raw_static_eval`).
- Existing P0-72 quiet beta-cutoff positive update path remains unchanged.
- No qsearch CorrectionHistory update was added.
- No direct MovePicker/pruning/LMR/probcut/singular/TT/UCI/NNUE behavior change was introduced.

## 15) P0-75 correction-history runtime delta hardening
- CorrectionHistory runtime update deltas are now centrally scaled and clamped via `search_params::correction_history_runtime_delta_scale` and `search_params::correction_history_runtime_delta_max`.
- The same bounded-delta policy applies to both existing runtime helpers:
  - quiet beta-cutoff positive helper,
  - fail-low negative helper.
- No new CorrectionHistory runtime update point was added.
- Main negamax hook locations are unchanged.
- No qsearch, MovePicker, pruning, LMR, TT, UCI, or NNUE behavior changes were introduced.

## 18) P0-76 search selectivity parameter foundation (zero-behaviour)
- Added centralized search-selectivity parameter foundation in `include/sirio/search_params.hpp` with disabled/no-op defaults for:
  - reverse futility,
  - move count pruning,
  - probcut,
  - singular extensions.
- P0-76 is infrastructure-only:
  - reverse futility remains disabled and not wired,
  - move count pruning remains disabled and not wired,
  - probcut remains disabled and not wired,
  - singular extensions remain disabled and not wired.
- No behaviour change was introduced in:
  - MovePicker,
  - qsearch,
  - TT store/probe/replacement,
  - UCI options/defaults,
  - NNUE runtime,
  - pruning/LMR/null-move/futility behaviour.
- Existing CorrectionHistory, Capture/NoisyHistory, and ContinuationHistory contracts from prior P0 stages are unchanged.

## 18) P0-77 reverse futility decision helper foundation (disabled/no-op)
- Added reverse futility **decision helper foundation** only.
- Helper is guarded by the P0-76 selectivity flag (`selectivity_reverse_futility_enabled`) and is disabled by default.
- No active reverse futility pruning return was added to search runtime.
- No behavioural search change is introduced in this step.
- No move count pruning, probcut, singular extension, LMR, qsearch, MovePicker, TT, UCI, or NNUE behaviour changed in this step.

## 16) P0-78 reverse futility disabled probe wiring (main negamax, no-op)
- `should_apply_reverse_futility_pruning(...)` is now wired in main `negamax(...)` as a disabled probe-only call.
- Wiring location is after corrected static eval availability and before move generation/move loop entry, and remains outside qsearch.
- No active reverse-futility pruning return was added; probe result is explicitly no-op consumed.
- `selectivity_reverse_futility_enabled` remains `false`.
- No behaviour change was introduced.
- No move count pruning, probcut, singular extension, LMR, qsearch, MovePicker, TT, UCI, or NNUE behaviour was changed by P0-78.

## 15) P0-79 reverse futility disabled guarded return scaffold
- Added reverse futility guarded return scaffold in main `negamax(...)` only.
- Guard remains exclusively `should_apply_reverse_futility_pruning(...)`.
- `selectivity_reverse_futility_enabled` remains `false`, so guard is unreachable under defaults.
- No behaviour change under current defaults.
- No changes to move count pruning, probcut, singular extensions, LMR, qsearch, MovePicker, TT, UCI, or NNUE behaviour.

## 18) P0-80 reverse futility safety predicate expansion (still disabled)
- Reverse futility helper safety predicate contract was expanded to include explicit PV/root/check/depth safety gates for future activation.
- `selectivity_reverse_futility_enabled` remains `false`; reverse futility remains disabled by default.
- P0-79 guarded reverse-futility return scaffold in main `negamax` remains present and unreachable under defaults.
- No behaviour change in current defaults.
- No move count pruning, probcut, singular extension, LMR, qsearch, MovePicker, TT, UCI, or NNUE behaviour changed in this step.

## 18) P0-82 reverse futility return observability (disabled/no-behaviour)
- Added deterministic observability for the guarded reverse futility return path in main negamax.
- `selectivity_reverse_futility_enabled` remains `false`, so guarded return remains unreachable under current defaults.
- No behaviour change under defaults.
- No move count pruning, probcut, singular extension, LMR, qsearch, MovePicker, TT, UCI, or NNUE behaviour changed.

## P0-83 Reverse Futility Conservative Activation (Main Negamax Only)
- Reverse futility pruning is now enabled conservatively in main negamax.
- Existing safety gates are preserved: not in check, not PV node, not root node, depth > 0, and depth within `reverse_futility_depth_limit`.
- Existing centralized margin helper `reverse_futility_margin(depth, improving)` remains the guard margin source.
- Existing return observability is preserved via `record_reverse_futility_return()` at the guarded return.
- qsearch remains excluded (no reverse futility helper call and no reverse-futility return scaffold in qsearch).
- Move count pruning, probcut, and singular extensions remain disabled.
- No direct behaviour change was made to LMR, MovePicker, TT, UCI options/defaults, or NNUE runtime.

## P0-84 update (move count pruning helper foundation only)
- Added centralized move count pruning helper foundation in search parameters.
- `selectivity_move_count_pruning_enabled` remains `false`.
- No active move count pruning return/continue/break was added to main negamax.
- Reverse futility P0-83 guarded behavior remains unchanged.
- Probcut and singular extensions remain disabled.
- No LMR, qsearch, MovePicker, TT, UCI, or NNUE behavior changed.

## 18) P0-85 Move Count Pruning disabled probe wiring
- Move Count Pruning helper is now wired into main negamax as a disabled no-op probe.
- `selectivity_move_count_pruning_enabled` remains `false`.
- No `continue`/`break`/`return` was added for Move Count Pruning.
- No move is skipped by this wiring.
- Reverse futility P0-83 path remains unchanged.
- Probcut and singular extensions remain disabled.
- No LMR, qsearch, MovePicker, TT, UCI, or NNUE behavior changed in this step.


## 29) P0-86 move count pruning guarded continue scaffold (disabled)
- Added a guarded Move Count Pruning continue scaffold in main `negamax(...)` move loop behind `should_apply_move_count_pruning(...)`.
- `selectivity_move_count_pruning_enabled` remains `false`, so the guarded continue is unreachable under current defaults.
- No behavior change was introduced.
- Reverse futility (P0-83) remains unchanged and enabled under its current settings.
- Probcut and singular extensions remain disabled.
- No LMR, qsearch, MovePicker, TT, UCI, or NNUE behavior changed.

## 18) P0-87 Move Count Pruning continue observability (disabled/no-behaviour contract)
- Added deterministic observability for the guarded main-negamax Move Count Pruning continue path.
- `selectivity_move_count_pruning_enabled` remains `false`.
- The guarded MCP continue path remains unreachable under current defaults.
- No behaviour change was introduced.
- Reverse futility P0-83 behavior remains unchanged.
- Probcut and singular extensions remain disabled/not wired.
- No LMR, qsearch, MovePicker, TT, UCI, or NNUE behavior changed.

## P0-88 move count pruning move-safety predicate expansion (disabled contract preserved)
- Move Count Pruning helper safety predicate contract was expanded with explicit move-safety inputs.
- Explicit guards now require quiet, non-promotion, and non-tactical/non-noisy move state before pruning can ever apply.
- `selectivity_move_count_pruning_enabled` remains `false`; MCP remains disabled/no-op under defaults.
- P0-86 guarded `continue` scaffold remains present and unreachable under defaults.
- P0-87 observability wiring remains intact (`record_move_count_pruning_continue()` remains inside the guarded continue block).
- No behavior change was introduced.
- Reverse futility (P0-83) remains unchanged.
- Probcut and singular extensions remain disabled.
- No LMR, qsearch, MovePicker, TT, UCI, or NNUE behavior changed.

## P0-89 Move Count Pruning threshold helper (disabled/no-behaviour-change)
- Added a centralized Move Count Pruning threshold helper in search parameters.
- Threshold calculation is now centralized and deterministic.
- `selectivity_move_count_pruning_enabled` remains `false`.
- P0-86 guarded continue scaffold remains unreachable under current defaults.
- P0-87 move-count-pruning observability remains intact.
- P0-88 quiet/non-promotion/non-tactical guards remain intact.
- No behaviour change.
- Reverse futility P0-83 remains unchanged.
- ProbCut and singular extensions remain disabled.
- No LMR, qsearch, MovePicker, TT, UCI, or NNUE behaviour changed.

## 18) P0-90 conservative MCP constants / disabled no-op contract
- Conservative Move Count Pruning threshold constants were set explicitly for future activation readiness.
- `selectivity_move_count_pruning_enabled` remains `false`.
- MCP remains disabled in production behavior.
- P0-86 guarded `continue` remains unreachable under defaults.
- P0-87 observability wiring remains intact.
- P0-88 move-safety guards remain intact.
- P0-89 threshold helper remains centralized and deterministic.
- No behavior change is introduced by P0-90.
- Reverse futility P0-83 remains unchanged.
- Probcut and singular extensions remain disabled.
- No LMR, qsearch, MovePicker, TT, UCI, or NNUE behavior changed.

## P0-91 update — Move Count Pruning conservative activation
- Move Count Pruning is now enabled conservatively in main negamax.
- P0-88 quiet/non-promotion/non-tactical guards are preserved.
- P0-90 conservative thresholds are preserved.
- P0-87 observability before continue is preserved.
- qsearch remains excluded from MCP helper/scaffold/observability wiring.
- Reverse futility P0-83 remains unchanged.
- Probcut and singular extensions remain disabled.
- No LMR, MovePicker, TT, UCI, or NNUE behavior was directly changed.

## 18) P0-93 ProbCut disabled main-negamax probe wiring
- ProbCut helper is now wired into main negamax as a disabled no-op probe only.
- `selectivity_probcut_enabled` remains `false`.
- No ProbCut search, reduced-depth search, or return was added.
- Reverse futility P0-83 remains unchanged.
- Move Count Pruning P0-91 remains unchanged.
- Singular extensions remain disabled.
- No LMR, qsearch, MovePicker, TT, UCI, or NNUE behavior changed.

## 29) P0-95 ProbCut probe observability (disabled/no-op contract preserved)
- Added deterministic observability for the P0-93 guarded ProbCut probe path in main `negamax` via `SearchHistory::record_probcut_probe()`.
- `selectivity_probcut_enabled` remains `false`; the guarded path remains disabled/no-op under defaults.
- P0-93 probe location is preserved in main negamax; P0-94 threshold/reduction helpers remain unchanged.
- No ProbCut search, reduced-depth search, return, or cutoff behavior was added.
- Reverse futility P0-83 remains unchanged.
- Move Count Pruning P0-91 remains unchanged.
- Singular extensions remain disabled.
- No LMR, qsearch, MovePicker, TT, UCI, or NNUE behavior changed.

## P0-96 ProbCut guarded parameter scaffold (disabled/no-op contract preserved)
- Added guarded ProbCut parameter preparation inside the existing P0-95 main-negamax probe block.
- Guarded block now computes ProbCut beta threshold and reduced depth via centralized P0-94 helpers.
- `selectivity_probcut_enabled` remains `false`.
- No ProbCut search, reduced-depth search, return, or cutoff was added.
- P0-95 observability remains intact via `record_probcut_probe()`.
- Reverse futility P0-83 remains unchanged.
- Move Count Pruning P0-91 remains unchanged.
- Singular extensions remain disabled.
- No LMR, qsearch, MovePicker, TT, UCI, or NNUE behavior changed.
