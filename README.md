[![CI](https://github.com/akurilin/cui/actions/workflows/ci.yml/badge.svg)](https://github.com/akurilin/cui/actions/workflows/ci.yml)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/akurilin/cui)

# Project description

This project is focused on building a reusable UI kit in C using SDL3, currently targeting macOS. The sample app currently includes two pages:

- `todo` (`todo_page`): the primary sample application showing realistic widget composition and interactions.
- `corners` (`corners_page`): a resize/anchor validation page with eight edge/corner anchored buttons.

## Current UI

Captured on February 14, 2026 from the current TODO sample page:

![Main page UI screenshot (2026-02-14)](assets/screenshots/main-page-2026-02-14.png)

Captured on February 18, 2026 from the corners page as a showcase of anchoring behavior when the window is resized:

![Corners page anchoring screenshot (2026-02-18)](assets/screenshots/corners-page-anchoring-2026-02-18.png)

SDL and SDL_image are brought in as Git submodules at `vendored/SDL` and
`vendored/SDL_image`.

## Architecture Overview

The codebase is split into:

- a reusable UI kit (`include/ui`, `src/ui`) for concrete widgets and shared element primitives,
- a UI system/runtime layer (`include/system`, `src/system`) for orchestration and dispatch,
- an example app layer (`include/pages`, `src/pages`) hosted by a small application shell (`main.c`).

- `main.c` is composition/root wiring only: parse startup flags, select a page by id, create window/renderer, initialize `ui_runtime`, create the active page, and run the main loop.
- `todo_page` is the sample TODO app and owns todo-specific model state plus screen-level UI composition.
- `corners_page` is a lightweight anchor test screen used to verify corner/edge placement during resize.
- `ui_runtime` is the lifecycle owner + dispatcher for all elements.
- `ui_element` is the common base interface for polymorphism in C.

### UI "Inheritance" Model (C-style)

There is no language-level inheritance in C, so this project uses struct embedding + a virtual function table:

- Every concrete widget embeds `ui_element base;` as its first field.
- Every concrete widget installs a `ui_element_ops` table (`handle_event`, optional `hit_test`,
  optional `can_focus`, optional `set_focus`, `update`, `render`, `destroy`).
- `ui_runtime` stores all widgets as `ui_element *` and calls the ops table, which gives runtime polymorphism similar to a base-class interface.

Inheritance chain in this project:

```text
ui_element (base type)
  -> ui_window
  -> ui_pane
  -> ui_button
  -> ui_checkbox
  -> ui_text
  -> ui_text_input
  -> ui_image
  -> ui_slider
  -> ui_segment_group
  -> ui_hrule
  -> ui_layout_container
  -> ui_scroll_view
  -> ui_fps_counter
```

### Layout & Sizing Model

The UI uses a **top-down width, bottom-up height** convention:

- **Width flows down**: parent elements push widths onto children by writing `child->rect.w`. A `ui_scroll_view` sets the `ui_layout_container`'s `x`/`w`, and the container in turn sets each child's `x`/`w`.
- **Height flows up**: children own their heights (`rect.h` is set at creation or during update). Parents read `child->rect.h` to position subsequent children and to auto-size their own `rect.h` to fit content.

`ui_layout_container` supports both **vertical** and **horizontal** stacking. In vertical mode, children are positioned top-to-bottom and the container stretches each child's width to fill; in horizontal mode, children are positioned left-to-right and the container stretches each child's height. For horizontal rows, right-aligned children keep `rect.x` as a right-edge inset instead of participating in left-flow x placement. Layout uses fixed 8 px padding and 8 px inter-child spacing.

Layout is **single-pass and imperative** — there are no separate measure/arrange phases. The container runs its layout pass on every `handle_event` and `update` call so that size changes propagate within the same frame. This keeps the implementation small and easy to follow.

Children track a **parent pointer and alignment anchors** for relative positioning. `ui_element_screen_rect()` resolves each element's window-space rectangle by walking the parent chain and applying horizontal/vertical anchor modes. This enables reliable anchoring (for example, bottom-right HUD elements) while preserving explicit ownership through container/context registration.

**The cascade in practice** (sidebar example): `ui_scroll_view` sets the container's `x`/`w` → `ui_layout_container` sets each child's `x`/`w` → container reads children's `h` to auto-size → scroll view reads the container's `h` to determine scroll bounds.

Key files:

- `include/pages/app_page.h`: generic page-ops interface plus build-generated page table declarations.
- `include/pages/corners_page.h`, `src/pages/corners_page.c`: resize-anchor test page with eight edge/corner-aligned buttons.
- `include/pages/todo_page.h`, `src/pages/todo_page.c`: todo page public lifecycle API + private page logic (task state, callbacks, and widget composition).
- `CMakeLists.txt` (page discovery): scans `src/pages/*_page.c` and generates `build/generated/page_index.c`, which exports `app_pages[]` for runtime page selection.
- `include/ui/ui_element.h`, `src/ui/ui_element.c`: base type, virtual ops contract, and shared border helpers.
- `include/system/ui_runtime.h`, `src/system/ui_runtime.c`: dynamic element list, ownership, event/update/render dispatch.
- `include/ui/ui_pane.h`, `src/ui/ui_pane.c`: rectangle fill + border visual group element.
- `include/ui/ui_button.h`, `src/ui/ui_button.c`: clickable element with press/release semantics and callback.
- `include/ui/ui_checkbox.h`, `src/ui/ui_checkbox.c`: labeled toggle control with boolean change callback.
- `include/ui/ui_text.h`, `src/ui/ui_text.c`: static debug-text element.
- `include/ui/ui_hrule.h`, `src/ui/ui_hrule.c`: thin horizontal divider line with configurable inset.
- `include/ui/ui_image.h`, `src/ui/ui_image.c`: image element with fallback texture behavior.
- `include/ui/ui_slider.h`, `src/ui/ui_slider.c`: horizontal slider with min/max range and value callback.
- `include/ui/ui_text_input.h`, `src/ui/ui_text_input.c`: single-line text field with ui_runtime-managed focus, keyboard input, and submit callback.
- `include/ui/ui_segment_group.h`, `src/ui/ui_segment_group.c`: segmented control (radio-button group) with selection callback.
- `include/ui/ui_layout_container.h`, `src/ui/ui_layout_container.c`: vertical/horizontal stack container with auto-sizing.
- `include/ui/ui_scroll_view.h`, `src/ui/ui_scroll_view.c`: scrollable viewport wrapper with mouse-wheel input and clip-rect rendering.
- `include/ui/ui_fps_counter.h`, `src/ui/ui_fps_counter.c`: self-updating FPS label anchored to viewport bottom-right.
- `include/ui/ui_window.h`, `src/ui/ui_window.c`: non-rendering root parent element used for window-relative anchoring.

### Frame/Lifecycle Flow

Per frame, `main.c` drives the UI system in this order:

1. Poll SDL events and forward each to `ui_runtime_handle_event`.
2. Call selected page `update()` for page-level per-frame work (for example, header clock refresh in `todo_page`).
3. Call `ui_runtime_update(delta_seconds)`.
4. Clear renderer and call `ui_runtime_render(renderer)`.
5. Present frame.

`ui_runtime` behavior rules:

- Dispatch `handle_event` only for `enabled` elements.
- Route text and keyboard events only to the currently focused element.
- Route pointer events by front-to-back hit testing and stop at first handler.
- Capture pointer interaction on left press and release capture on left release.
- Dispatch `update` only for `enabled` elements.
- Dispatch `render` only for `visible` elements.
- Destroy all registered elements via each element's `destroy` op during `ui_runtime_destroy`.

### Page Discovery & Startup Selection

- Page ids are derived from filenames using `src/pages/<id>_page.c`.
- CMake discovers all `*_page.c` files and generates `build/generated/page_index.c`.
- Each page exports `<id>_page_ops` (for example, `todo_page_ops`, `corners_page_ops`).
- `main.c` selects the startup page with `--page <id>`.
- `--help` prints the currently discovered page ids.

### Startup vs Resize Control Flow

These are two distinct flows in the current system.

#### 1) First launch: how elements get their initial size/position

1. `main.c` parses startup options (`--page`, `--width`, `--height`), resolves the page descriptor from build-generated `app_pages[]`, initializes SDL + window + renderer, initializes `ui_runtime`, then calls selected page `create(window, &context, width, height)`.
2. `todo_page_create` stores viewport dimensions and computes top-level geometry (content width, header widths, list height, footer positions).
3. Most top-level widgets are created with explicit rects derived from those computed values (header, input row, rules, list frame, footer, etc.).
4. The task list body is built as:
   - `ui_scroll_view` (viewport/clipping),
   - containing a vertical `ui_layout_container` (`rows_container`),
   - containing one horizontal `ui_layout_container` per task row.
5. Each task row creates leaf controls (number, checkbox, title, time, delete button) with fixed row/column dimensions.
6. Page-owned elements are registered into `ui_runtime` (`ui_runtime_add`), transferring ownership.
7. During frame updates, `ui_scroll_view` and `ui_layout_container` run layout passes:
   - scroll view positions/stretches child content to viewport width,
   - layout containers compute child x/y/w/h (vertical or horizontal stacking),
   - vertical containers auto-size content height from children.
8. Absolute render/hit-test rects are resolved from parent-relative rects via parent chain + alignment anchors (`ui_element_screen_rect`).

In short: initial geometry is seeded by page code, then refined every frame by container layout passes.

#### 2) Runtime resize: user drags window

1. SDL emits `SDL_EVENT_WINDOW_RESIZED`.
2. `main.c` handles it by:
   - updating renderer logical presentation to the new logical size,
   - calling selected page `resize(page_instance, new_w, new_h)`.
3. `todo_page_resize` (for the TODO page implementation) updates page viewport fields and calls `relayout_page`.
4. `relayout_page` recomputes and writes top-level rects:
   - stretches/shifts header panes,
   - right-anchors add/filter controls,
   - resizes list frame + scroll view + rows container,
   - repositions footer elements,
   - updates window-root/fps anchoring dimensions.
5. Event processing and the subsequent `ui_runtime_update` pass trigger container layout again, so row internals and scroll bounds are recalculated under the new dimensions.
6. `ui_runtime_render` then draws the updated layout for that frame.

In short: resize updates page-level geometry immediately, and container-based child layout is re-applied during event/update passes before render.

### Ownership Rules

- Element constructors (`ui_button_create`, `ui_pane_create`, etc.) allocate on the heap and return ownership to caller.
- After `ui_runtime_add` succeeds, ownership transfers to `ui_runtime`.
- `todo_page_create` registers page elements in `ui_runtime`; on any partial failure it rolls back and destroys already-registered page elements before returning `NULL`.
- `todo_page_destroy` removes and destroys elements that were registered by the page, then frees page-owned task/model storage.

## Roadmap

This project intentionally stays minimal. The following capabilities are explicitly out of scope:

1. **Real text rendering** — Widgets currently use `SDL_RenderDebugText` (fixed 8x8 monospace glyphs), and we do not plan to add full font loading/rendering systems.
2. **Keyboard navigation and focus traversal** — Focus/capture routing exists in `ui_runtime`, but tab ordering, arrow-key traversal, and focus ring rendering are not planned.
3. **Styling and theming** — Widget colors/padding/spacing remain construction-time values; a shared theme/style inheritance system is not planned.

Potential future enhancements (still under consideration):

1. **Tree-aware propagation and overlay layers** — Event routing is hit-tested with focus/capture today, but explicit parent/child target paths, bubble/capture phases, and overlay layers could improve modals, dropdowns, tooltips, and context menus.
2. **Richer layout primitives** — Layout is currently vertical/horizontal stacking with fixed 8 px padding/spacing. Proportional/weighted sizing, min/max constraints, grid layout, alignment control, and configurable spacing may be added later.
3. **Dropdown/select control** — A segmented control exists today, but a true dropdown/select widget (button + popup list + overlay/z-order handling + outside-click dismissal) is still missing.
4. **Constraint-style responsive layout** — Anchors and stack stretching are implemented, but full responsive constraints (min/max sizing rules, percentage/fill policies, and window-relative pin/stretch behavior across arbitrary element trees) are not.
5. **Data-driven UI description (markup/DSL)** — UI is currently composed in C only. A simple declarative format (for example JSON/TOML/custom DSL) plus loader/binder would make screen iteration and reuse faster.
6. **Animation system with easing curves** — There is no built-in tween/animation layer yet. Adding time-based transitions (position, size, color, opacity) with reusable easing curves would improve interaction quality.
7. **Broader automated test coverage** — CTest currently runs a focused hierarchy test binary. Wider coverage is still needed for widget behavior, focus/input routing, layout/resize regressions, and integration-level page scenarios.
8. **Sample-page use of all core widgets** — `ui_image` and `ui_slider` are implemented but not yet integrated into the TODO sample page, so there is no end-to-end in-app demonstration of those controls.

## Configure and Build:
```
cmake -S . -B build
cmake --build build
```

## Makefile shortcuts:
```
make build    # configure + build
make test     # build + run CTest suite
make run      # build + run build/Debug/cui, build/Release/cui, or build/cui (use RUN_ARGS/ARGS for app flags)
make clean    # remove build directory
make format   # apply clang-format to non-vendored .c/.h files
make lint     # run clang-tidy checks
make analyze  # run Clang Static Analyzer for the cui target via scan-build
make precommit # run all commit-gating checks
make install-hooks # enable repo-managed Git hooks
make submodules-init   # initialize and fetch vendored submodules
make submodules-update # update submodules to latest remote commits
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
The executable is generated under the build configuration directory. Depending
on your generator/configuration, run one of:

```
./build/Debug/cui
./build/Release/cui
./build/cui
```

Optional startup window size:

```
./build/cui --width <width> --height <height>
```

Optional startup page id (`<id>` is derived from `src/pages/<id>_page.c`):

```
./build/cui --page <id>
```

Example:

```
./build/cui --page todo -w 800 -h 600
```

Corners anchor test page:

```
./build/cui --page corners
```

Show command-line help:

```
./build/cui --help
```

Pass app args through `make run`:

```bash
make run RUN_ARGS="--page todo --width 800 --height 600"
```

`ARGS` is also supported as an alias:

```bash
make run ARGS="--page corners --width 1000 --height 700"
```

## Screenshot Capture (macOS)

Use the helper script to launch the app, wait for startup, and capture the app window:

```bash
scripts/capture_app_window.sh ./build/cui /tmp/cui-example.png
```

Pass app arguments (for example startup width/height) after `--`:

```bash
scripts/capture_app_window.sh ./build/cui /tmp/cui-example-800x600.png 2 -- --page todo --width 800 --height 600
```

## Submodule workflow

Use Makefile commands:

```bash
make submodules-init
make submodules-update
```
