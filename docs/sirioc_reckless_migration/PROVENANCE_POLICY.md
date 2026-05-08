# SirioC Provenance and Originality Policy

## Source originality
- SirioC source remains original unless an explicit relicensing decision is made and documented.

## External engine usage boundaries
- External engines may be used only as black-box opponents or optional teachers in future documented experiments.
- No direct code import, translation, or adaptation from external engines/trainers without an explicit licence and governance decision.

## Weights and artefact distribution
- No external weights may be redistributed without documented licence review.
- Any redistribution decision must include provenance, licence terms, and compatibility notes.

## Required metadata for future ML artefacts
- Every future network must include a `MODEL_CARD.md`.
- Every future dataset must include a `DATASET.md`.
- Every generated network artefact must include:
  - checksum,
  - training commit,
  - dataset manifest,
  - training configuration.
