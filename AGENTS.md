# Agent Instructions

Future coding agents working in this repository must follow these rules:

1. Never add gameplay unless explicitly asked.
2. Do not change existing behavior during refactor tasks.
3. Do not rewrite algorithms unless needed for compilation.
4. Do not rename public fields or functions unless explicitly requested.
5. Keep the project compiling after every small step.
6. Never include `.c` files from other `.c` files.
7. Headers should contain declarations, typedefs, enums, structs, and function prototypes.
8. Source files should contain implementations and private static helpers.
9. Rendering code must not mutate simulation state.
10. UI code should update configuration or issue commands, not directly rewrite world generation or simulation internals.
11. World generation must not depend on UI, rendering, or civilization simulation.
12. Simulation may depend on world data, but world generation must not depend on simulation.
13. Prefer small refactor steps over one large rewrite.
14. Update the build command or Makefile when adding source files.
15. Keep every `.c` and `.h` file at 500 lines or less. If a file grows past that, split it by responsibility before adding more work.
16. Before editing, produce a function-to-module plan. If code discovery invalidates the plan, stop and revise the plan before continuing.
17. New files should not include `core/game_types.h` unless they truly need legacy global state. Prefer narrower headers such as `core/constants.h`, `core/world_types.h`, `core/sim_types.h`, or module-specific headers.
18. If the user asks for discussion, analysis, a prompt, or explicitly says not to edit code, do not modify files or run builds. Inspect code only when needed, then produce findings, a plan, or an executable prompt.
19. Before code search, build, test, Python scripts, `make`, or `build.bat` on Windows, prepend `C:\msys64\ucrt64\bin;C:\Users\c4esa\AppData\Local\Programs\Python\Python313` to `PATH`, then verify `gcc` and `python` resolve to the expected local tools.
20. Player-visible logs must use structured event log entries with localization and stable civilization identity snapshots. Do not add new raw English `event_log_push` messages for gameplay, performance, diplomacy, expansion, collapse, plague, or debug events.
21. UI and rendering paths should prefer `RenderSnapshot` or cached read models over live simulation globals. Do not hold simulation/state locks during expensive rendering, text layout, event formatting, map cache rebuilds, or other UI-only work.
22. Any change adding per-frame, per-month, per-civilization, or per-pair work must identify whether it scans tiles, regions, cities, routes, or all civilization pairs. Hot-path full-map scans require dirty flags, caching, batching, or scheduler budgeting.
23. New UI text must use the existing localization helpers and UTF-8-safe rendering path. Do not paste mojibake strings. Verify Chinese labels render correctly when the UI language is Chinese.
24. If build output such as `world_sim.exe` is locked, do not kill processes, delete files, or force-unlock handles unless explicitly approved. Report the lock and use the approved build workflow.
25. Keep gameplay constants, balance values, and feature rules in design or documentation files unless the user explicitly asks to encode them in repository instructions.

## Code Review Rules

The tracked code review source of truth is `docs/unofficial/ver0.1.4a_code_review_notes_for_codex.pdf`.
Its version metadata is recorded in `docs/unofficial/ver0.1.4a_code_review_notes_for_codex.version.md`.

When performing code review in this repository:

1. Treat the PDF rules as binding review instructions.
2. Review like an architecture reviewer, not a feature designer.
3. Do not add new gameplay or suggest new gameplay unless it is required to fix architecture.
4. Do not make code changes during review unless the user explicitly asks for implementation.
5. First produce a review report focused on architecture, module boundaries, maintainability, file size, and build consistency.
6. Check that no `.c` or `.h` file exceeds 500 lines; if one does, recommend a responsibility-based split.
7. Report each issue with file, problem, why it matters, suggested solution, urgency, and whether it is refactor-only or gameplay-changing.
8. Before each review, check whether the PDF or its version metadata changed. If the PDF changed, re-read it before reviewing.
9. If any PDF instruction conflicts with this file or is unclear, ask the user before guessing.
