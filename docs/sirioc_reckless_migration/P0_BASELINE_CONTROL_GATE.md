# P0 Baseline Control Gate

This document records SirioC's current baseline before any functional migration work.

## Baseline architecture snapshot (repository-derived)
- UCI engine core lives in `src/` and `include/sirio/` with board, movegen, search, evaluation, timing, TT, and Syzygy integration modules.
- NNUE runtime backend is implemented under `src/nnue/` and `include/sirio/nnue/`.
- NNUE training pipeline is self-contained under `training/nnue/`.
- Benchmark and utility tooling exists under `bench/` and `tools/`.

## Current NNUE status (legacy/test baseline)
- Network file header is `SirioNNUE1` in text format, parsed by `SingleNetworkBackend::load`.
- `FeatureState` is piece-count based (`std::array<int, kFeatureCount>`), where `kFeatureCount = 2 * PieceType::Count`.
- `NetworkParameters` currently contains `bias`, `scale`, and `piece_weights`.
- `SingleNetworkBackend` evaluation is linear: bias + dot(piece_weights, piece_counts), then scaled and rounded.
- This `SingleNetworkBackend` path is baseline legacy/test-only for future phases and must not be treated as the target competitive NNUE architecture.

## Current search baseline (`src/search.cpp` visible features)
- Iterative deepening with aspiration windows.
- Alpha-beta negamax with quiescence search.
- Transposition table probing/storage.
- Move ordering including TT move, tactical ordering, killer moves, and history heuristic.
- Late move reductions (LMR) table-driven reductions.
- Null-move pruning.
- Futility pruning (non-check branch guard).
- Draw detection through fifty-move and threefold repetition checks.
- Syzygy probing in root, main search, and quiescence paths under configured limits.
- Threaded search orchestration with shared stop/time/node state.

No additional search features should be claimed unless directly evidenced in `src/search.cpp`.

## Current training baseline
- Training scripts currently present:
  - `training/nnue/scripts/prepare_dataset.py`
  - `training/nnue/scripts/dataset.py`
  - `training/nnue/scripts/features.py`
  - `training/nnue/scripts/train.py`
  - `training/nnue/scripts/evaluate.py`
  - `training/nnue/scripts/export_to_engine.py`
  - `training/nnue/scripts/match_orchestrator.py`
- Current baseline model/pipeline is piece-count centric; the 12-feature `PieceCountModel`-class approach is legacy for future migration planning.

## Future phase order (control plan)
- **P0-01:** extract search parameters into `include/sirio/search_params.hpp` without behaviour change.
- **P0-02:** extract history containers into `include/sirio/history.hpp` and `src/history.cpp` without search-strength tuning.
- **P0-03:** define SirioNNUE2 binary format and feature-index contract, without enabling it as default.
- **P0-04:** implement SirioNNUE2 loader/inference behind explicit tests.
- **P0-05:** replace training pipeline with sparse-feature dataset/export scaffolding.

## Explicit non-goals for this gate
- No NNUE strength claims.
- No Elo claims.
- No Reckless code port.
- No Stockfish net compatibility.
- No Lc0 embedding.
- No teacher-distillation implementation yet.
