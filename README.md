[![CI](https://github.com/akurilin/cui/actions/workflows/ci.yml/badge.svg)](https://github.com/akurilin/cui/actions/workflows/ci.yml)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/akurilin/cui)

# Project description

This project attempts to create a UI system built entirely in the C language and leveraging SDL, targeting specifically macOS. It's a learning project to master the fundamentals of UI building and the C language starting from the low level, letting SDL take care of some of the foundations, but taking over the rest of it.

SDL and SDL_image are brought in as Git submodules at `vendored/SDL` and
`vendored/SDL_image`.

## Architecture Overview

The app is split into a tiny application layer (`main.c`) and a reusable UI layer (`include/ui`, `src/ui`).

- `main.c` is composition/root wiring only: create window/renderer, construct UI elements, register them with a `ui_context`, then run the main loop.
- `ui_context` is the lifecycle owner + dispatcher for all elements.
- `ui_element` is the common base interface for polymorphism in C.

### UI "Inheritance" Model (C-style)

There is no language-level inheritance in C, so this project uses struct embedding + a virtual function table:

- Every concrete widget embeds `ui_element base;` as its first field.
- Every concrete widget installs a `ui_element_ops` table (`handle_event`, `update`, `render`, `destroy`).
- `ui_context` stores all widgets as `ui_element *` and calls the ops table, which gives runtime polymorphism similar to a base-class interface.

Inheritance chain in this project:

```text
ui_element (base type)
  -> ui_pane
  -> ui_button
  -> ui_checkbox
  -> ui_text
  -> ui_image
  -> ui_slider
  -> ui_fps_counter
```

Key files:

- `include/ui/ui_element.h`, `src/ui/ui_element.c`: base type, virtual ops contract, and shared border helpers.
- `include/ui/ui_context.h`, `src/ui/ui_context.c`: dynamic element list, ownership, event/update/render dispatch.
- `include/ui/ui_pane.h`, `src/ui/ui_pane.c`: rectangle fill + border visual group element.
- `include/ui/ui_button.h`, `src/ui/ui_button.c`: clickable element with press/release semantics and callback.
- `include/ui/ui_checkbox.h`, `src/ui/ui_checkbox.c`: labeled toggle control with boolean change callback.
- `include/ui/ui_text.h`, `src/ui/ui_text.c`: static debug-text element.
- `include/ui/ui_image.h`, `src/ui/ui_image.c`: image element with fallback texture behavior.
- `include/ui/ui_slider.h`, `src/ui/ui_slider.c`: horizontal slider with min/max range and value callback.
- `include/ui/ui_fps_counter.h`, `src/ui/ui_fps_counter.c`: self-updating FPS label anchored to viewport bottom-right.

### Frame/Lifecycle Flow

Per frame, `main.c` drives the UI system in this order:

1. Poll SDL events and forward each to `ui_context_handle_event`.
2. Call `ui_context_update(delta_seconds)`.
3. Clear renderer and call `ui_context_render(renderer)`.
4. Present frame.

`ui_context` behavior rules:

- Dispatch `handle_event` only for `enabled` elements.
- Dispatch `update` only for `enabled` elements.
- Dispatch `render` only for `visible` elements.
- Destroy all registered elements via each element's `destroy` op during `ui_context_destroy`.

### Ownership Rules

- Element constructors (`ui_button_create`, `ui_pane_create`, etc.) allocate on the heap and return ownership to caller.
- After `ui_context_add` succeeds, ownership transfers to `ui_context`.
- On failed add, caller remains responsible (see `add_element_or_fail` in `main.c`).

## Configure and Build:
```
cmake -S . -B build
cmake --build build
```

## Makefile shortcuts:
```
make build    # configure + build
make run      # build + run ./build/Debug/hello
make clean    # remove build directory
make format   # apply clang-format to non-vendored .c/.h files
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
The executable is generated under the build configuration directory:

```
./build/Debug/hello
```

## Submodule workflow

Clone with submodules:

```bash
git clone --recurse-submodules <repo-url>
```

If you already cloned the repo:

```bash
git submodule update --init --recursive
```

When pulling new commits from this repo, also refresh submodules:

```bash
git pull --recurse-submodules
```

If submodules show as modified and you want to reset them to the commits pinned by this repo:

```bash
git submodule update --init vendored/SDL vendored/SDL_image
```
