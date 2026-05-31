# Ver0.2.12.a Side Doc

Ver0.2.12.a is a UI/UX Claymorphism foundation checkpoint over Ver0.2.12. It
keeps gameplay, simulation, world generation, map rules, diplomacy, war, plague,
population, economy, technology, and balance behavior unchanged while adding
the first presentation-only clay UI foundation.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.2.12.a`.
2. Added `src/ui/ui_clay_theme.h` and `src/ui/ui_clay_theme.c` for centralized
   Claymorphism theme tokens.
3. Added `src/ui/ui_clay_primitives.h` and
   `src/ui/ui_clay_primitives.c` for reusable clay surface drawing primitives.
4. Connected the side-panel shell to the clay panel primitive.
5. Connected panel tabs to the clay tab primitive.
6. Updated panel-tab text color to use `ui_clay_text_color(state)`.
7. Updated `Makefile` and `build.bat` for the new UI clay source files.
8. Added AGENTS rule 43, documenting hwnd-scoped non-disruptive GUI validation
   with methods such as `SWP_NOACTIVATE`, `PrintWindow`, and safe direct window
   messages, while prohibiting focus-stealing global input unless explicitly
   approved by the user.

## Behavioral Notes

- This release is a foundation and minimal shell proof only.
- Existing side-panel content, tabs, controls, debug rows, top bar, bottom bar,
  and map body rendering are intended to remain unchanged.
- Clay drawing logic is centralized in `ui_clay_*` modules rather than copied
  into individual panel files.
- The current primitives create GDI pens and brushes per draw. This is
  acceptable for the small Phase 1 proof, but broader migration should add a
  widget or surface-cache layer before expanding the style to many controls.

## Known Follow-Up

- Phase 2 can begin after this checkpoint and should focus on top/bottom bars,
  buttons, tabs, and pause menu.
- Surface caching or a widget layer should be considered before broadening
  repeated clay shadows and stateful controls.
- Phase 6 large-map performance remains outside this release scope.

## Validation

- `WORLD_SIM_VERSION` is `0.2.12.a`.
- `MAP_SAVE_VERSION` remains 10.
- Phase 1 build/static validation passed before release, and focused
  non-disruptive GUI smoke validation confirmed side-panel shell, tabs, panel
  content, Debug / Performance rows, and Chinese text remained visible.
- Strict AGENTS full game-flow regression was not completed for this
  presentation-only checkpoint.
