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
16. Before editing, produce a function-to-module plan. Then implement that exact plan.
17. New files should not include `core/game_types.h` unless they truly need legacy global state. Prefer narrower headers such as `core/constants.h`, `core/world_types.h`, `core/sim_types.h`, or module-specific headers.

## Code Review Rules

The tracked code review source of truth is `docs/ver0.1.4a_code_review_notes_for_codex.pdf`.
Its version metadata is recorded in `docs/ver0.1.4a_code_review_notes_for_codex.version.md`.

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
