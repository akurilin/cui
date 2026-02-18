# Agent Instructions

## Source Of Truth
- Read `README.md` before making changes.
- Treat `README.md` as the single source of truth for architecture, module/file layout, build/run commands, test/validation workflows, and tool usage.
- Do not duplicate architecture or workflow documentation in this file.
- If your changes affect structure, ownership/lifecycle flow, file locations, or developer workflow, update `README.md` in the same change.

## Coding Expectations
- Language: C (C11-compatible style preferred).
- Indentation: 4 spaces, no tabs.
- Braces: Allman style for functions, control flow, and aggregate declarations.
- Naming: `snake_case` for functions/variables; choose descriptive file and symbol names.
- Function names should start with a verb and clearly describe behavior.
- Keep functions small and purpose-driven; add comments only where logic is non-obvious.

## Refactor And Comment Hygiene
- During refactors, moves, or renames, preserve existing comments when they remain accurate.
- If behavior, ownership, control flow, naming, or API contracts change, update or remove stale comments in the same change.
- For new/changed public declarations in headers, document purpose, behavior/contract, parameters, return value, and ownership/lifecycle expectations.

## Commit Hygiene
- Keep commits focused on one logical change.
- Use imperative, concise commit subjects.
- If changes are unrelated, split them into separate commits.

## UI Change Validation
- After any UI-related code change, verify the visual result at the end of the task.
- Run `scripts/capture_app_window.sh` to capture the app window, then compare the captured image against the expected UI outcome before finishing.
- Save validation screenshots to `/tmp` (for example, `/tmp/cui-check.png`), not to `assets/`.
- Treat these captures as ephemeral one-time checks and delete any accidental validation screenshots created under `assets/`.
