# Dataset schema

Prepared datasets are stored as a pair of files inside `training/nnue/datasets/processed/`:

1. `<name>.npz` – A NumPy archive that contains the arrays needed for training:
   - `features`: `(N, 12)` float32 matrix with piece counts for *white pawns → kings*
     followed by the same order for black.  Values are normalised to `[0, 1]` by
     dividing raw counts by 8.
   - `targets`: `(N,)` float32 vector with the centipawn score from the point of
     view of the side to move.
   - `ply`: `(N,)` uint16 vector with the half-move count for bookkeeping.
2. `<name>.metadata.json` – Describes how the dataset was created (PGN sources,
   sampling stride, target recipe, seed and example counts).

The schema is intentionally simple so that the same arrays can be consumed by
PyTorch, NumPy or custom tooling.

## Feature order

The 12 feature slots follow the same ordering as `sirio::PieceType`:

```
[White Pawn, White Knight, White Bishop, White Rook, White Queen, White King,
 Black Pawn, Black Knight, Black Bishop, Black Rook, Black Queen, Black King]
```

This matches the layout expected by `SingleNetworkBackend::compute_state` in
`src/nnue/backend.cpp`.

## Normalisation

The training scripts expect piece counts to be normalised by the maximum number
of copies of a piece on the board (8 for pawns, 10 for minor/major pieces but we
stick to 8 for all slots for simplicity).  Targets are expressed in centipawns
(i.e. pawn units × 100).

## Example metadata file

```json
{
  "name": "sirio_example",
  "version": 1,
  "seed": 20240511,
  "source": ["data/games.pgn"],
  "samples": 128000,
  "sample_stride": 2,
  "result_weight": 400.0,
  "target": "material+result",
  "created_at": "2024-05-11T10:32:00Z"
}
```

