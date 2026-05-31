# Ver0.2.12.b Side Doc

Ver0.2.12.b is a UI/UX Claymorphism Phase 2A and resource-hygiene checkpoint
over Ver0.2.12.a. It keeps gameplay, simulation, world generation, map rules,
diplomacy, war, plague, population, economy, technology, and balance behavior
unchanged while extending the presentation-only clay UI work and fixing the
Windows resource build path.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.2.12.b`.
2. Added `src/ui/ui_clay_widgets.h` and `src/ui/ui_clay_widgets.c` as the
   reusable clay widget layer for buttons, pill buttons, icon buttons, and menu
   panels.
3. Extended `src/ui/ui_clay_primitives.*` with pill and inset pill drawing.
4. Migrated the top-bar reset/language buttons, bottom play/speed buttons,
   side-panel handle, and pause-menu shell/buttons to the Phase 2A clay widget
   path.
5. Fixed the collapsed side-panel handle square artifact by avoiding the opaque
   collapsed mini-panel cache path and drawing the tiny handle directly over the
   existing map scene.
6. Fixed `build.bat` so it builds `src/world_sim.rc` with `windres` and links
   `build/world_sim_resource.o`, matching the Makefile resource behavior and
   preserving the application icon after both build paths.
7. Added `world_sim.exe --no-activate` for validation-only non-activating GUI
   launches, while preserving the default `run_game()` / `SW_SHOW` behavior.

## Behavioral Notes

- This release is UI/resource focused. It is not a gameplay, simulation,
  worldgen, diplomacy, war, plague, technology, or balance checkpoint.
- Existing hit tests, actions, panel tabs, panel content, debug rows, map body
  rendering, labels, and controls are intended to remain visible and unchanged
  except for the targeted Claymorphism presentation styling.
- The `--no-activate` path exists to support validation without stealing focus
  from the user's foreground application.
- `build.bat` now preserves the same app icon resource that the Makefile build
  already preserved.

## Known Follow-Up

- Phase 6 large-map performance and 5x interaction stutter remain outside this
  release scope.
- Broader Claymorphism migration should add cached surfaces or other GDI object
  reuse before expanding styling across high-frequency panels.
- Future performance/stutter work must diagnose with Debug / Performance panel
  evidence and pass the strict AGENTS performance regression before acceptance.

## Validation

- `WORLD_SIM_VERSION` is `0.2.12.b`.
- `MAP_SAVE_VERSION` remains 10.
- Build/static validation and focused non-disruptive GUI validation were
  completed by the UI/UX agent for the Phase 2A UI/resource scope.
- App icon preservation was validated after both `make -B world_sim.exe` and
  `cmd /c build.bat`.
- The collapsed side-panel handle was validated over the map with no square
  dark patch or stale hover pixels.
- Strict AGENTS full game-flow regression was not completed for this
  presentation/resource checkpoint.
