# Contributing to SirioC

Welcome to the SirioC project! We appreciate your interest in advancing our
academic UCI chess engine. This guide outlines how to build the project, report
issues, and submit improvements in a manner consistent with our standards.

## Table of Contents

- [Building SirioC](#building-sirioc)
- [Making Contributions](#making-contributions)
  - [Reporting Issues](#reporting-issues)
  - [Submitting Pull Requests](#submitting-pull-requests)
- [Code Style](#code-style)
- [Community and Communication](#community-and-communication)
- [License](#license)

## Building SirioC

SirioC is implemented in modern C++ and builds on Windows, macOS, and Linux. If
you require a compiler toolchain, consult the documentation for your platform or
the instructions provided in the repository wiki. On Unix-like systems the
standard build process is:

```bash
cd src
make -j profile-build
```

Running `make help` lists all supported targets together with the intended CPU
architectures and optimisation profiles.

## Making Contributions

### Reporting Issues

Bug reports should be filed via the issue tracker with the following
information:

- Operating system and compiler version.
- Exact build configuration and command line used.
- Minimal steps to reproduce the problem.
- Expected behaviour versus observed behaviour.

### Submitting Pull Requests

- Functional changes must be validated through Sequential Probability Ratio
  Tests (SPRT) executed in the [FastChess](https://github.com/fastchess/fastchess)
  framework. Provide a summary of the testing conditions and results in the pull
  request description.
- Non-functional changes (refactoring, documentation, tooling) should explain
  why they are performance neutral; add tests if behaviour could be affected.
- Each pull request must include a concise summary of the motivation and the
  technical approach.
- First-time contributors are encouraged to add their names to the
  [`AUTHORS`](AUTHORS) file.

## Code Style

SirioC follows the formatting rules specified in [.clang-format](.clang-format).
You can automatically format your changes by running `make format`, which
requires `clang-format` version 18.

## Community and Communication

- Join our forthcoming community channels to discuss research directions and
  implementation details.
- Share empirical results and theoretical insights to encourage collaborative
  progress.

## License

By contributing to SirioC you agree that your work will be released under the
GNU General Public License v3.0. The full license text is available in
[Copying.txt](Copying.txt).

Thank you for contributing to SirioC and helping us refine the engine.

