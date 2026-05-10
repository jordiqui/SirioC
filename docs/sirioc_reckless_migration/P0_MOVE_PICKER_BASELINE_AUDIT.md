# P0-47 — MovePicker Scoring Baseline Audit / No-Behaviour-Change Contract

## 1) Scope
This deliverable is **documentation-only** and establishes a precise baseline for current MovePicker and ordering behaviour before any new history/correction/selectivity integration.

- No behaviour change.
- No search tuning.
- No Elo/strength claim.

## 2) MovePicker location and ownership
### Files and symbols inspected
- `src/search.cpp`
  - `class MovePicker`
  - `int mvv_lva_score(const Move&)`
  - `int static_exchange_score(const Board&, const Move&)`
  - `negamax(...)` (MovePicker call-site and history updates)
  - `quiescence(...)` (tactical MovePicker call-site)
- `include/sirio/search.hpp`
- `include/sirio/history.hpp`
- `src/history.cpp`
- `include/sirio/search_params.hpp`
- `src/movegen.cpp`
- `include/sirio/move.hpp`
- `src/tt.cpp` and `include/sirio/transposition_table.hpp` (TT move context only)
- `docs/sirioc_reckless_migration/P0_SEARCH_REENTRY_BASELINE.md`

### Ownership/layout
- MovePicker is currently **embedded in `src/search.cpp`** as an internal class in the anonymous namespace, not externalized to a dedicated module/header.
- MovePicker dependencies are currently:
  - `Board` (for SEE via `static_exchange_score`).
  - `SearchContext` (for killer/history access).
  - `SearchHistory` (`killer_slots`, `quiet_history_score`).
  - `Move` representation (`captured`, `promotion`, `is_en_passant`, `is_castling`).
  - TT best move handoff as `std::optional<Move>` from `negamax` TT probe path.
  - Search constants from `search_params` (MVV values).

## 3) Current move ordering inputs (as implemented today)
Only in-tree observed inputs are listed:

- **TT move**: yes (if TT-provided move is legal and present in generated move list).
- **Captures / tactical captures**: yes.
- **Quiet moves**: yes.
- **Promotions handled separately**: yes (`promotions_` stage distinct from captures/quiets).
- **Killers**: yes (2 slots per ply, read from `SearchHistory`).
- **History heuristic**: yes, **quiet history only** (`quiet_history_[color][from][to]`).
- **SEE/static exchange score**: yes, used for capture partition/scoring and main-search tactical skip gate.
- **MVV/LVA**: yes, blended with SEE into capture priority.
- **Counter-move history**: absent.
- **Continuation history**: absent.
- **Noisy/capture history**: absent.
- **Correction history**: absent.

## 4) Current scoring stages (observed order)
Current MovePicker staging/order is:

1. **TT move** (`Stage::TT`), if candidate exists and matches a generated legal move.
2. **Good captures** (`Stage::GoodCaptures`): captures/en-passant with `SEE >= 0`, sorted by descending combined priority.
3. **Promotions** (`Stage::Promotions`): sorted by descending promotion-piece value.
4. **Killers** (`Stage::Killers`, non-tactical mode only): matched against the two killer slots for current ply.
5. **Quiets** (`Stage::Quiets`, non-tactical mode only): sorted by descending quiet-history score.
6. **Bad captures** (`Stage::BadCaptures`): captures/en-passant with `SEE < 0`, sorted descending by same priority formula.

Scoring formulas in current code:
- Capture priority: `priority = (see_score << 10) + mvv_lva_score(move)`.
- MVV/LVA term: `victim_value * 100 - attacker_value`.
- Promotion priority: `promoted_piece_value * 100`.
- Quiet priority: `quiet_history_score(move, mover)`.

Additional ordering-related search gates outside MovePicker:
- In `negamax`, non-TT tactical moves can be **skipped** at shallow depth when `SEE < 0` and not a promotion (`depth_left <= 5`).
- In `quiescence`, tactical moves with `SEE < 0` are skipped.

No additional fallback/randomized ordering stage is present; remaining ordering is deterministic from staged lists and per-stage sort order.

## 5) Existing history usage baseline
### History tables that exist
In `SearchHistory` today:
- `quiet_history_[2][64][64]` integer table.
- `killer_moves_[max_search_depth][2]` optional move slots.

### Where history is read
- `MovePicker` reads killer slots via `context.history.killer_slots(ply)`.
- `MovePicker` reads quiet-history score via `context.history.quiet_history_score(move, mover)` for quiet move sorting.

### Where history is updated
In `negamax`:
- Quiet history updated per searched quiet move:
  - success update on fail-high (`score >= beta`),
  - success update on improving alpha over original (`score > alpha_original`),
  - failure update otherwise.
- Killer storage on quiet beta cutoff (`alpha >= beta`).

### Present vs absent history families
- Present: quiet history, killer moves.
- Absent: noisy history, capture history, continuation history, counter-move history, correction history.

## 6) Risk map for future changes
- **Capture/noisy history integration**: **High risk** — directly perturbs tactical ordering and cutoff distribution; likely to reshape node topology quickly.
- **Continuation history integration**: **High risk** — adds path-dependent ordering feedback; sensitive to indexing and update policy.
- **Correction history with eval/TT interaction**: **Medium-High risk** — eval bias changes bound outcomes and TT reuse patterns.
- **SEE threshold changes**: **High risk** — both MovePicker partitioning and tactical skip gates depend on SEE sign/threshold.
- **Killer/history interaction**: **Medium-High risk** — stage placement versus quiet-history rank affects quiet cutoff frequency.
- **TT move priority interaction**: **High risk** — TT-first ordering materially affects PVS behaviour, cutoffs, and effective branching.
- **LMR dependency on move index/order**: **High risk** — current LMR triggers by `move_index` and quietness; any ordering drift changes reduction application map.

## 7) Proposed safe follow-up order (planning only)
No implementation in this patch. Recommended controlled sequence:

1. **P0-48**: MovePicker ordering test harness / deterministic ordering snapshot (no behaviour change).
2. **P0-49**: capture/noisy history data-structure scaffold (no search use).
3. **P0-50**: continuation history scaffold (no search use).
4. **P0-51**: correction history scaffold (no search use).
5. Later: controlled MovePicker integration of **one** signal at a time.
6. Later: LMR retuning only after ordering tests are stable.
7. Later: ProbCut / singular extensions.

## 8) Guardrails
- One behavioural search change per patch maximum.
- Every behavioural MovePicker change must include before/after deterministic ordering tests.
- No simultaneous NNUE and search behaviour changes.
- No fastchess/OpenBench before explicit approval.
- No Elo claims from unit tests.
- No copying code from Reckless, Stockfish, Berserk, or other engines.
- Inspiration-only policy; SirioC implementation must remain original.

## 9) Validation suite (stable command list)
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

P0-47 is documentation-only. Search behaviour is unchanged. NNUE behaviour is unchanged. SirioNNUE2 remains non-default. No strength/Elo claim is made.

# P0-48 — MovePicker Ordering Snapshot Harness / No-Behaviour-Change Contract

## Harness path
- `tests/move_picker_snapshot_tests.cpp`
- test adapter symbol in `src/search.cpp`: `move_picker_order_snapshot_for_tests(...)`

## FEN coverage
- Starting position: `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1`
- Tactical capture-rich: `4k3/8/3q4/3p4/3P4/4N3/8/4K3 w - - 0 1`
- Quiet middlegame-like: `r2q1rk1/pp2bppp/2n1pn2/2bp4/2P5/2NP1NP1/PP2PPBP/R1BQ1RK1 w - - 0 1`
- Promotion-like static: `4k3/6P1/8/8/8/8/8/4K3 w - - 0 1`
- Captures+quiets with TT move input: `8/8/3k4/3p4/3P4/8/3K4/8 w - - 0 1` with TT move `d2e3`

## Snapshot status
- Exact full ordered snapshot asserted for:
  - starting position,
  - tactical capture-rich position,
  - promotion-like static position,
  - TT-move priority position.
- Deterministic repeated-order + legality/invariant assertions asserted for:
  - quiet middlegame-like position.

## Assertions covered
- repeated calls produce identical ordering,
- number of picked moves equals expected snapshot size (where full snapshots are asserted),
- first move check (implicit via full list and explicit front check),
- full ordered list equality (selected FENs),
- no duplicate moves,
- all picked moves are legal in the source position,
- TT move priority behaviour checked when supplied and legal.

## Access notes / limitations
- MovePicker remains embedded in `src/search.cpp` (anonymous namespace).
- No broad refactor was done.
- A minimal test-only adapter function was added in `src/search.cpp` to invoke current production MovePicker path with deterministic zero/default `SearchContext`/`SearchHistory` state.
- Quiet middlegame test currently enforces deterministic and legal ordering invariants without a pinned full list to reduce brittleness for large quiet partitions while still preserving a deterministic no-behaviour-change contract.

## Behavioural continuity confirmations
- No MovePicker scoring change.
- No search/negamax/quiescence/LMR/TT/eval/UCI/NNUE runtime change.
- No strength/Elo claim.

# P0-49 — Capture/Noisy History Data Structure Scaffold / No-Behaviour-Change Contract

## Scope
- Added capture/noisy history scaffolding inside `SearchHistory`.
- No MovePicker scoring changes.
- No search-side read/write integration yet.

## Added structures
- `SearchHistory::CaptureHistory`
  - Indexing dimensions: `[mover_color][attacker_piece][captured_piece][to_square]`.
  - Intended for capture-class moves only (`move.captured.has_value()`).
- `SearchHistory::NoisyHistory`
  - Indexing dimensions: `[mover_color][moving_piece][to_square]`.
  - Intended for non-quiet moves (`!is_quiet_move(move)`), including promotions/captures/castling/en-passant.

Both scaffolds provide:
- zero-initialized default state,
- deterministic indexing from `Move` + mover color only,
- bounded update with existing history limits (`history_bonus_limit`, `history_min`, `history_max`),
- `clear()` reset to zero.

## Tests added
- Extended `tests/history_tests.cpp` with isolated scaffold coverage:
  - zero-default checks,
  - positive and negative updates,
  - bonus clamping and saturation,
  - clear/reset behavior,
  - deterministic indexing behavior and non-collision checks for distinct capture keys,
  - deterministic repeated updates.

## Deferred integration
- Capture/noisy history is **not** read in MovePicker.
- Capture/noisy history is **not** updated from `negamax` or `quiescence`.
- Future integration into ordering/search is deferred to later P0 steps.

# P0-50 — Continuation History Data Structure Scaffold / No-Behaviour-Change Contract

## Scope
- Added continuation history scaffolding inside `SearchHistory`.
- No MovePicker scoring changes.
- No search-side read/write integration.

## Added structure
- `SearchHistory::ContinuationHistory`
  - Indexing dimensions: `[previous_mover_color][current_mover_color][previous_moving_piece][previous_to_square][current_moving_piece][current_to_square]`.
  - Minimal explicit scaffold keyed only from move-context pairs and mover colors.
  - No dependency on global mutable state.

Provided APIs:
- `score(...)` query.
- `update(...)` bounded by existing history controls (`history_bonus_limit`, `history_min`, `history_max`).
- `clear()` reset to zero.

## Tests added
- Extended `tests/history_tests.cpp` for continuation-history scaffold coverage:
  - zero-default checks,
  - positive/negative updates,
  - saturation/clamping,
  - reset via `ContinuationHistory::clear()`,
  - deterministic indexing and non-collision checks across distinct keys,
  - deterministic repeated updates,
  - `SearchHistory::clear()` resetting continuation history.

## Deferred integration
- Continuation history is **not** read in MovePicker.
- Continuation history is **not** updated from `negamax` or `quiescence`.
- Integration into ordering/search remains deferred to later P0 steps.

# P0-51 — Correction History Data Structure Scaffold / No-Behaviour-Change Contract

## Scope
- Added correction-history scaffolding inside `SearchHistory`.
- No MovePicker scoring changes.
- No search-side or evaluation-side read/write integration.

## Added structure
- `SearchHistory::CorrectionHistory`
  - Indexing dimensions: `[mover_color][bucket]`.
  - Current scaffold key scheme uses an explicit fixed bucket index (`bucket % 1024`) plus mover color.
  - This is intentionally a minimal deterministic placeholder until safe pawn/material hash keys are wired for later integration.

Provided APIs:
- `score(Color mover, std::size_t bucket)` query.
- `update(Color mover, std::size_t bucket, int depth, bool success)` bounded by existing history controls (`history_bonus_limit`, `history_min`, `history_max`).
- `clear()` reset to zero.

## Tests added
- Extended `tests/history_tests.cpp` for correction-history scaffold coverage:
  - zero-default checks,
  - positive/negative updates,
  - saturation/clamping,
  - reset via `CorrectionHistory::clear()`,
  - deterministic indexing, bucket-isolation, and wrap behavior (`bucket + 1024` aliasing by modulo),
  - deterministic repeated updates,
  - `SearchHistory::clear()` resetting correction history.

## Deferred integration
- Correction history is **not** read in MovePicker.
- Correction history is **not** read in evaluation.
- Correction history is **not** read or updated from `negamax` or `quiescence`.
- Integration into ordering/eval/search remains deferred to later P0 steps.

# P0-52 — SearchHistory Lifecycle Integration Audit / No-Behaviour-Change Contract

## Scope
- Added aggregate lifecycle contract tests for `SearchHistory` as a complete owned state bundle.
- Validation-only step; no search-path consumption of new history tables.

## Aggregate lifecycle validated
The aggregate contract now validates lifecycle behavior for all currently owned tables:
- killer history,
- quiet history,
- capture history,
- noisy history,
- continuation history,
- correction history.

Coverage confirms:
- default/zero initialization,
- non-zero updates after writes,
- `SearchHistory::clear()` resets all owned tables,
- key isolation under mixed updates,
- deterministic repeated clear/update cycles.

## Integration status
- No new history table is read in `MovePicker`.
- No new history table is updated from `negamax`/`quiescence`.
- No MovePicker scoring change in this step.
- Future behavioral integration remains deferred.
