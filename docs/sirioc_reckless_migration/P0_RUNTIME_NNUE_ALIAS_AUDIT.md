# P0-40 — SirioNNUE2 Legacy Runtime Alias Audit (No-Behaviour-Change)

## 1) Scope and no-behaviour-change statement
This deliverable is a runtime-surface **audit/documentation task only** for NNUE naming and aliasing.

**No runtime behaviour changes are made in P0-40.**
- No UCI option name changes.
- No UCI default value changes.
- No alias removals.
- No loader semantic changes.
- No search/evaluation routing changes.
- No SirioNNUE1/SirioNNUE2 behavioural changes.

## 2) Runtime-visible NNUE option/alias inventory

### A) `EvalFile`
- **Current source file:** `src/uci.cpp` + registration baseline in `include/sirio/uci_options.hpp`.
- **Current purpose:** Primary user-facing NNUE file path option.
- **Current behaviour:** Stores string path; callback updates pending runtime NNUE load path.
- **Compatibility risk:** Medium. Name is conventional and stable, but defaults use `.nnue` names that may be interpreted as generic/Stockfish-compatible by users.
- **Stockfish `.nnue` compatibility claim:** No.
- **Recommended future action:** **keep temporarily**; **document as legacy naming surface** until explicit UCI migration plan.

### B) `EvalFileSmall`
- **Current source file:** `src/uci.cpp`.
- **Current purpose:** Secondary/small NNUE path surface for compatibility/workflow continuity.
- **Current behaviour:** Stores string path with callback path update; runtime contract remains unchanged.
- **Compatibility risk:** Medium. The option is runtime-visible and filename style can be interpreted as Stockfish-style even when format compatibility is not claimed.
- **Stockfish `.nnue` compatibility claim:** No.
- **Recommended future action:** **keep temporarily**; **document as legacy/ambiguous naming**; **rename later** only after explicit approval.

### C) `UseNNUE`
- **Current source file:** `src/uci.cpp`.
- **Current purpose:** Boolean runtime gate for enabling/disabling NNUE path.
- **Current behaviour:** Preserves existing enable/disable semantics; no changes in this audit.
- **Compatibility risk:** Low. Broadly standard name; mostly clear.
- **Stockfish `.nnue` compatibility claim:** No.
- **Recommended future action:** **keep temporarily**; revisit only if/when SirioNNUE2-default migration requires split controls.

### D) `NNUEFile` (legacy alias surface)
- **Current source file:** `src/uci.cpp`.
- **Current purpose:** Alias/compatibility path option mapped to existing EvalFile workflow.
- **Current behaviour:** Callback keeps alias behaviour; no semantic changes.
- **Compatibility risk:** Medium-High. Alias can obscure which option is canonical and can reinforce ambiguous `.nnue` expectations.
- **Stockfish `.nnue` compatibility claim:** No.
- **Recommended future action:** **keep temporarily**; **document as legacy alias**; **remove later after explicit migration** with grace period/tests/release notes.

## 3) Default NNUE filename inventory and classification

### A) `nn-1c0000000000.nnue`
- **Current source file(s):** `src/uci.cpp`, `include/sirio/uci_options.hpp`, runtime docs surfaces (`README.md`, `docs/uci_options.md`, GUI docs).
- **Classification:**
  - SirioNNUE1 legacy text format: **runtime expects SirioNNUE1 loader contract when used as active net unless separately detected/handled**.
  - SirioNNUE2 binary: **No**.
  - ambiguous/Stockfish-style-looking: **Yes** (hash-like `.nnue` naming convention).
  - documentation-only: **No** (runtime default and docs reference).
- **Compatibility risk:** Medium-High due to naming ambiguity.
- **Stockfish `.nnue` compatibility claim:** No.
- **Recommended future action:** **document as legacy/ambiguous now**; **rename later** under approved UCI migration plan.

### B) `nn-37f18f62d772.nnue`
- **Current source file(s):** `src/uci.cpp` (`EvalFileSmall` default), docs references (`README.md`, GUI/CCRL docs).
- **Classification:**
  - SirioNNUE1 legacy text format: **same legacy-path ambiguity as above**.
  - SirioNNUE2 binary: **No**.
  - ambiguous/Stockfish-style-looking: **Yes**.
  - documentation-only: **No** (runtime-visible secondary default + docs).
- **Compatibility risk:** Medium-High for same reasons.
- **Stockfish `.nnue` compatibility claim:** No.
- **Recommended future action:** **keep temporarily**; **document as legacy/ambiguous**; evaluate eventual rename/removal with explicit migration approval.

### C) `tests/data/minimal.nnue`
- **Current source file(s):** `tests/data/minimal.nnue`, referenced by NNUE format/runtime tests.
- **Classification:**
  - SirioNNUE1 legacy text format: **Yes**.
  - SirioNNUE2 binary: **No**.
  - ambiguous/Stockfish-style-looking: **Low** (test fixture context).
  - documentation-only: **No** (test contract artifact).
- **Compatibility risk:** Low (internal test fixture).
- **Stockfish `.nnue` compatibility claim:** No.
- **Recommended future action:** **keep temporarily** as legacy continuity fixture until explicit runtime migration gates retire it.

### D) `.nnue2` candidate/export names (`candidate.nnue2`, `golden_path.nnue2`, etc.)
- **Current source file(s):** v2 pipeline scripts/tests (`training/nnue/scripts/*_v2.py`, `tests/*v2*`).
- **Classification:**
  - SirioNNUE1 legacy text format: **No**.
  - SirioNNUE2 binary: **Yes (pipeline/test artifacts)**.
  - ambiguous/Stockfish-style-looking: **No**.
  - documentation-only: **No** (tooling/test artifact names).
- **Compatibility risk:** Low; suffix is Sirio-specific for v2 pipeline.
- **Stockfish `.nnue` compatibility claim:** No.
- **Recommended future action:** **replace with SirioNNUE2-specific runtime option later** when runtime-default migration is explicitly approved.

## 4) Runtime-visible reporting surfaces audited
- `src/nnue/api.cpp` reports format labels (`SirioNNUE1Legacy`, `SirioNNUE2MinimalV1`, etc.) and metadata strings (legacy status, detector diagnostic, dimensions).
- `include/sirio/nnue/api.hpp` exposes the public detection enum/metadata structures.
- `README.md` and `training/nnue/README.md` already state no Stockfish `.nnue` compatibility claim and legacy baseline framing.
- Existing test surfaces validate fake Stockfish-style payload rejection and legacy fixture detection continuity.

## 5) Explicit statements required by P0-40
- **SirioC does not currently claim Stockfish `.nnue` compatibility.**
- **SirioNNUE2 remains non-default.**
- **Runtime behaviour is unchanged by this audit.**

## 6) Proposed future staged cleanup (non-binding)
1. **Documentation first:** mark canonical runtime option names and legacy aliases more explicitly in user docs.
2. **Optional warning/reporting second:** add non-breaking reporting/warnings only after approval.
3. **UCI option migration later:** rename/deprecate aliases only with explicit roadmap approval.
4. **Compatibility grace period:** support old/new names in parallel for at least one release cycle.
5. **Removal gates:** require tests + release notes + migration note before alias/default retirement.

## 7) Validation commands run
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

## 8) Strength/Elo claim statement
This patch is audit/documentation only and makes **no Elo or strength claim**.

## P0-41 follow-up note
- Reporting clarification has been added on existing NNUE `format_report` surfaces:
  - `stockfish_nnue_compatibility=not_claimed`
  - `sirio_nnue1_nnue_names=legacy_sirio_format`
  - `sirio_nnue2_runtime_status=non_default`
- This is reporting-only; no UCI option/default, loader semantic, or runtime activation/default change was made.
