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
