# P0 Search Refactor Log

## Task
- **Task name:** P0-01 Search Parameter Extraction.
- **Behaviour-preserving statement:** This patch is a structural modularisation only; numeric values, types, and search semantics are preserved intentionally.

## Constants/parameters extracted
- `mate_score`
- `max_search_depth`
- `mate_threshold`
- `mvv_values`
- `see_piece_values`
- `max_lmr_depth`
- `max_lmr_moves`
- `node_flush_interval`
- `info_output_lock_timeout`
- `time_check_interval`
- `max_search_threads` (moved from local `kMaxSearchThreads` value)
- `history_bonus_limit`
- `history_max`
- `history_min`
- `futility_margin_depth1` (moved from depth-1 local futility margin value)

All extracted constants were moved from `src/search.cpp` to `include/sirio/search_params.hpp` with identical values.

## Numeric literals intentionally left in `src/search.cpp`
- LMR table formula coefficients and transforms (e.g. `1.95`, log/round usage): kept in place to avoid changing reduction-table construction semantics.
- Deeply embedded heuristics and control literals used inline in formulas/conditions (e.g. aspiration window initialization `25`, null-move reduction terms, TT logic gates, bounds checks): intentionally unchanged in place for auditability and to avoid semantic drift in this extraction-only step.

## Files changed
- `include/sirio/search_params.hpp` (new)
- `src/search.cpp`
- `docs/sirioc_reckless_migration/P0_SEARCH_REFACTOR_LOG.md` (new)

## Validation commands run
- `git status --short`
- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
- `cmake --build build -j`
- `ctest --test-dir build --output-on-failure`
- `./build/sirio_tests`
- `./build/sirio_bench`

## Test results
- Recorded after running the validation commands listed above.

## Known limitations
- This task does not attempt full literal extraction of every inline numeric in `src/search.cpp`; deeply embedded literals were left intentionally per behaviour-preserving scope.


## Task
- **Task name:** P0-02 Search History Extraction.
- **Behaviour-preserving statement:** This patch is a structural modularisation only; history-related state, formulas, limits, and ordering semantics are preserved intentionally.

## History structures/functions extracted
- Extracted to `include/sirio/history.hpp` and `src/history.cpp`:
  - `SearchHistory` container owning:
    - killer move slots (`[max_search_depth][2]`)
    - quiet-history table (`[2][64][64]`)
  - `is_quiet_move(const Move&)`
  - `quiet_history_score(const Move&, Color)`
  - `update_quiet_history(Color, const Move&, int depth, bool success)`
  - `store_killer(const Move&, int ply)`
  - `killer_slots(int ply)` accessor for read-only MovePicker use
- Preserved formulas and limits:
  - bonus = `depth * depth`, clamped by `search_params::history_bonus_limit`
  - success/failure saturation to `search_params::history_max`/`search_params::history_min`

## History-related code intentionally left in `src/search.cpp`
- MovePicker integration and move-order staging remain in `src/search.cpp`; only data container/operations moved.
- Call sites within `negamax` remain in place and now delegate to `context.history` methods to preserve lifecycle and threading semantics.

## Files changed
- `include/sirio/history.hpp` (new)
- `src/history.cpp` (new)
- `src/search.cpp`
- `CMakeLists.txt`
- `docs/sirioc_reckless_migration/P0_SEARCH_REFACTOR_LOG.md`

## Confirmation statements
- No continuation history was added.
- No noisy history was added.
- No correction history was added.
- No new history heuristic was implemented.
- No search tuning was performed.

## Validation commands run
- `git status --short`
- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
- `cmake --build build -j`
- `ctest --test-dir build --output-on-failure`
- `./build/sirio_tests`
- `./build/sirio_bench`

## Test results
- Recorded after running the validation commands listed above.

## Known limitations
- No dedicated new history tests were added; validation relies on existing engine tests/bench executable.

## Task
- **Task name:** P0-02B SearchHistory Dedicated Tests.
- **Behaviour-preserving statement:** This is a test-only completion of P0-02; no SearchHistory logic, search behaviour, or engine heuristics were changed.

## Test files added/modified
- Added: `tests/history_tests.cpp`
- Modified: `tests/board_tests.cpp` (register/invoke `run_history_tests()`)
- Modified: `CMakeLists.txt` (compile new test translation unit into `sirio_tests`)

## Exact behaviours covered
- Fresh `SearchHistory` neutral quiet-history score for representative legal quiet move(s).
- Fresh killer slots default/empty state.
- `is_quiet_move` quiet detection and non-quiet rejection for a legal capture move.
- `update_quiet_history` depth-squared bonus semantics.
- `update_quiet_history` bonus clamping to `search_params::history_bonus_limit`.
- `update_quiet_history` saturation to `search_params::history_max` / `search_params::history_min` under repeated updates.
- `store_killer` first-slot store semantics.
- `store_killer` second distinct quiet move slot-shift semantics.
- `store_killer` duplicate re-store semantics (no duplicate churn when slot[0] matches).
- Isolation checks across side/from-to and across distinct plies.

## Behaviours intentionally not tested
- Non-quiet rejection path in `store_killer` for promotions/en-passant/castling was not separately asserted; this patch keeps coverage minimal and already validates non-quiet classification via legal capture construction and `is_quiet_move`.

## Confirmation statements
- No new history heuristic was implemented.
- No search tuning was performed.
- No continuation/noisy/correction history was added.

## Validation commands run
- `git status --short`
- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
- `cmake --build build -j`
- `ctest --test-dir build --output-on-failure`
- `./build/sirio_tests`
- `./build/sirio_bench`

## Test results
- Build and all requested validation commands completed successfully.
- `ctest` reported `100% tests passed (1/1)`.
- `./build/sirio_tests` reported `All tests passed.`
- `./build/sirio_bench` completed successfully.

## Known limitations
- Build emits existing upstream warnings from bundled third-party Fathom code (`third_party/fathom/tbprobe.c`/`tbcore.c` stringop-overflow warnings); this task does not modify third-party tablebase sources.
