# SirioC Agent Guardrails

Scope: entire repository.

## Engineering and language rules
- Use C++20 for all C++ implementation and interface changes.
- Preserve existing public APIs unless the task explicitly authorises an API change.
- Do not make behavioural changes outside the requested scope.

## Originality, licence, and provenance
- Do not copy code from GPL/AGPL engines, trainers, or projects.
- Keep SirioC originality explicit in design and implementation notes.
- Keep licence provenance explicit for all added or generated artefacts.

## Validation and reporting discipline
- Always run build and test commands after code changes when possible.
- If a command cannot be run, explain why; never report it as passed.

For every patch, report:
1. files changed,
2. exact scope,
3. commands run,
4. test results,
5. known limitations,
6. diffstat.
