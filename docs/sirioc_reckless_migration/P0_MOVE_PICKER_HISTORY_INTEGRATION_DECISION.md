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
- Updates are strictly gated by valid previous board+move context and valid continuation key extraction; no fallback semantics are introduced.
- No updates were added for qsearch, capture/noisy, promotion, failed/skipped/illegal paths.
- Zero-state MovePicker ordering contract remains protected.
