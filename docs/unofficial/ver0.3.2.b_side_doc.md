# Ver0.3.2.b Side Doc

Ver0.3.2.b is a focused UI/UX presentation checkpoint over Ver0.3.2.a. It keeps
the accepted vassal-action hotfix baseline while accepting Claymorphism Phase 3
for the World Setup presentation.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.2.b`.
2. Revalidated Phase 1, Phase 2A, and Phase 2B clay presentation coverage
   before applying Phase 3.
3. Added reusable clay helpers for section headers, input frames, sliders, and
   swatches.
4. Applied clay styling to World Setup section headers, map-size buttons,
   random buttons, sliders, input frames, civilization color preview controls,
   and swatches.
5. Applied clay panel/button styling to the color picker.
6. Added narrow hover and pressed state handling for the color picker Auto,
   Apply, and Cancel buttons.
7. Preserved native Win32 edit fields and Add/Apply child buttons to avoid
   changing input and focus behavior.

## Files In Scope

- `src/core/version.h`
- `src/render/panel_worldgen.c`
- `src/ui/color_picker.c`
- `src/ui/ui_clay_widgets.c`
- `src/ui/ui_clay_widgets.h`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.2.b_side_doc.md`

## Behavioral Notes

- This is a presentation-only UI/UX checkpoint.
- No world-generation rules, terrain rules, route rules, simulation behavior,
  vassal behavior, diplomacy, war, plague, population, economy, technology,
  save, or balance constants were intentionally changed.
- Native edit controls remain visually native/dark in the side panel.
- Add Civilization and Apply Selected remain native child controls.
- Color picker Apply and Cancel still close on mouse-down by existing behavior.
- `src/ui/color_picker.c` is now close to the 500-line limit and should be
  split before future substantial color-picker work.

## Validation

- `WORLD_SIM_VERSION` is `0.3.2.b`.
- Canonical `make -B world_sim.exe` succeeded.
- `cmd /c build.bat` succeeded.
- `make check-text` passed.
- `git diff --check` passed with only CRLF conversion warnings.
- Static checks found no `.c` file includes another `.c`, and all `.c` / `.h`
  files are at or below 500 lines.
- Root executable inventory contains exactly `world_sim.exe`.
- String checks found `World Sim Game Ver 0.3.2.b` in `world_sim.exe`.
- Focused UI/UX evidence captured an active `Generating world` progress overlay
  with overall progress, current stage, and progress bars.
- Focused UI/UX evidence covered World Setup controls, color picker
  hover/pressed states, generated political colors after generation,
  route-potential route-only legend, native child-control lifecycle,
  side-panel scrolling, pause menu, and Debug / Performance readability.
- The user approved the focused Phase 3 UI/UX checkpoint for push.

## Residual Risks

- Full strict AGENTS Large-map regression was not rerun for this focused UI/UX
  presentation checkpoint.
- Color picker hover/pressed differences are intentionally subtle within the
  clay visual style.
- Native Win32 edit fields and Add/Apply buttons remain less clay-styled than
  the surrounding presentation to preserve input and focus behavior.
