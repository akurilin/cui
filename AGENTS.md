# Repository Guidelines

## Project Structure & Module Organization
This repository is a minimal C + CMake starter focused on learning SDL-based UI development.

- `main.c`: current application entry point.
- `CMakeLists.txt`: canonical build configuration (targets, SDL linkage, output paths).
- `Makefile`: convenience wrapper around common CMake commands.
- `README.md`: setup and local workflow notes.
- `build/`: generated build artifacts (ignored by Git).
- `vendored/`: local third-party code checkout (currently ignored by Git).

Keep new source files near `main.c` until a larger structure is needed; then split into folders like `src/` and `include/`.

## Build, Test, and Development Commands
Use the Makefile for day-to-day work:

- `make build`: configure and compile the project.
- `make run`: build and run `./build/hello`.
- `make clean`: remove the build directory.

Equivalent raw CMake commands:

- `cmake -S . -B build`
- `cmake --build build`

## Coding Style & Naming Conventions
- Language: C (C11-compatible style preferred).
- Indentation: 4 spaces, no tabs in source files.
- Braces: K&R style (`int main(void) { ... }`), consistent with `main.c`.
- Naming: `snake_case` for functions/variables, descriptive filenames (for example, `ui_layout.c`).
- Keep functions small and purpose-driven; add brief comments only where logic is non-obvious.

No formatter or linter is configured yet. If one is introduced, document exact commands in this file and `README.md`.

## Testing Guidelines
No automated test framework is configured yet. For now, validate changes by:

1. `make build` (must compile cleanly)
2. `make run` (must execute without runtime errors)

When tests are added, place them under `tests/` and wire them into CMake/CTest (`ctest --test-dir build`).

## Commit & Pull Request Guidelines
Git history is currently minimal, so no strict convention is established yet. Use this going forward:

- Commit messages: imperative, concise subject (for example, `Add Makefile run target`).
- Keep commits focused on one logical change.
- PRs should include: what changed, why, how it was validated (`make build`/`make run`), and any follow-up work.
