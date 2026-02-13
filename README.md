# Project description

This project attempts to create a UI system built entirely in the C language and leveraging SDL, targeting specifically macOS. It's a learning project to master the fundamentals of UI building and the C language starting from the low level, letting SDL take care of some of the foundations, but taking over the rest of it.

SDL is currently brought in as a locally cloned repository under `vendored/SDL`:

```bash
git clone https://github.com/libsdl-org/SDL.git vendored/SDL
```

This is not configured as a proper Git submodule right now; it is just a nested Git repository used as a vendored dependency for local builds.

## Configure and Build:
```
cmake -S . -B build
cmake --build build
```

## Makefile shortcuts:
```
make build    # configure + build
make run      # build + run ./build/hello
make clean    # remove build directory
make format   # apply clang-format to main.c
make lint     # run clang-tidy checks
make analyze  # run Clang Static Analyzer via scan-build
make precommit # run all commit-gating checks
make install-hooks # enable repo-managed Git hooks
```

## Linting, Formatting, and Hooks:
This repository uses:

- `clang-format` for consistent formatting (`.clang-format`)
- `clang-tidy` for linting and static checks (`.clang-tidy`)
- `scan-build` for Clang Static Analyzer checks

Brace style policy: use Allman braces everywhere, meaning opening braces go on a new line for functions, control flow, and aggregate declarations.

To enforce checks before every commit:

```
make install-hooks
```

After that, Git will run `.githooks/pre-commit`, which executes `make precommit`.

On macOS, install required tools with Homebrew if needed:

```bash
brew install llvm
```

If Homebrew does not add LLVM binaries to your shell path automatically, add:

```bash
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
```

## Run:
The executable should be in the build directory:

```
cd build
./hello
```
