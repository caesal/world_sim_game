# Agent Instructions

Future coding agents working in this repository must follow these rules:

1. Never add gameplay unless explicitly asked.
2. Prefer small refactor steps.
3. Keep the project compiling after every change.
4. Do not rewrite algorithms during refactor tasks.
5. Rendering code must not mutate simulation state.
6. UI code must not directly mutate world generation internals.
7. World generation must not depend on UI, rendering, or civilization simulation.
8. Simulation may read world data but should not draw anything.
9. Do not include `.c` files from other `.c` files.
10. Update build commands when adding new source files.
