# NNUE Training Pipeline

This directory contains a self-contained and reproducible pipeline to train Sirio's
NNUE (Efficiently Updatable Neural Network) evaluation.  The goal of the pipeline
is to produce `SirioNNUE1` weight files that can be consumed by the engine at
runtime.

The pipeline is organised into three layers:

1. **Data preparation** – Convert PGN files into supervised learning examples
   that match Sirio's piece-count feature representation.
2. **Model training** – Optimise a light-weight neural network (bias, scale and
   2 × 6 piece weights) using the prepared dataset.
3. **Validation and export** – Evaluate the trained parameters on a holdout set
   and export them to the text format expected by `src/nnue/backend.cpp`.

## Quick start

```bash
# 1. Install Python dependencies
python -m venv .venv
source .venv/bin/activate
pip install -r training/nnue/requirements.txt

# 2. Prepare a dataset from PGN files
python -m training.nnue.scripts.prepare_dataset \
    --pgn data/games.pgn \
    --output-dir training/nnue/datasets/processed \
    --name sirio_example \
    --sample-stride 2 \
    --result-weight 400

# 3. Train the network using the provided YAML configuration
python -m training.nnue.scripts.train \
    --config training/nnue/configs/default.yaml

# 4. Export the best checkpoint to Sirio's runtime format
python -m training.nnue.scripts.export_to_engine \
    --checkpoint training/nnue/weights/sirio_example.pt \
    --output training/nnue/weights/sirio_example.nnue
```

The resulting `*.nnue` file can be loaded at runtime via the existing UCI option
or the `sirio::nnue::init` API.

## Directory layout

```
training/nnue/
├── configs/          # YAML files that describe datasets and training hyperparameters
├── datasets/         # Schema documentation and generated datasets (ignored by git)
├── metrics/          # Baseline validation results and future experiment logs
├── scripts/          # Python scripts that implement the pipeline
└── README.md         # This file
```

## Deterministic experiments

Every script accepts an explicit `--seed` (or reads one from the configuration).
Random operations (Python, NumPy and PyTorch) are seeded in a consistent manner
so that datasets, train/validation splits and weight initialisations are fully
reproducible.

## Baseline metrics

`metrics/baseline.json` stores the evaluation metrics that correspond to the
reference configuration shipped in `configs/default.yaml`.  Re-running the
pipeline with the same configuration and dataset should reproduce the same
numbers (modulo floating-point noise) on deterministic hardware.

## Extending the pipeline

- Add new dataset recipes by creating additional configuration files under
  `configs/` and pointing them to the generated `.npz` datasets.
- Plug-in alternative target functions (e.g. using engine self-play scores) by
  adjusting `prepare_dataset.py`.
- Experiment with multi-network policies by extending the model definition in
  `scripts/train.py` and updating the export script accordingly.

## Automated regression matches

Validated NNUE checkpoints can be stress-tested against the production
configuration with the orchestration script provided in
`scripts/match_orchestrator.py`.  The tool wraps either `cutechess-cli` or
`fastchess` and automates the full promotion cycle:

```bash
python -m training.nnue.scripts.match_orchestrator \
    --tool cutechess \
    --tool-path /usr/bin/cutechess-cli \
    --engine build/bin/sirio \
    --baseline-network training/nnue/weights/baseline.nnue \
    --validated-dir training/nnue/weights/candidates \
    --deploy-path runtime/nnue/current.nnue \
    --rounds 200 \
    --concurrency 4 \
    --time-control "60/5+0.5"
```

Each candidate found in `--validated-dir` is matched against the baseline
network and, if it exceeds the promotion threshold (default margin of
0.5 points), it is automatically copied to the deployment location.  Match
logs and results are persisted to `training/nnue/metrics/regressions/` so the
process can resume after interruptions.  Passing `--interval <seconds>` keeps
the orchestrator running indefinitely, polling the directory for new candidate
files.

