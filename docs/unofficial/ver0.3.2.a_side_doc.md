# Ver0.3.2.a Side Doc

Ver0.3.2.a is a hotfix checkpoint over Ver0.3.2. It keeps the accepted
direct-vassal row, Annex/Release action, city/harbor marker, and political
legend work while correcting user-reported regressions in vassal action pause
behavior, right-side panel wheel scrolling, and the active/fallen country list
toggle.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.2.a`.
2. Removed pause side effects from the direct-vassal Release and Annex request
   path.
3. Kept the selected-country Independence action on the same no-pause release
   request path.
4. Moved Country, World, and Debug side-panel wheel handling out of delayed
   map-zoom batching so scrolling responds immediately.
5. Changed the country-list toggle from active-plus-fallen to two exclusive
   filters: active-only and fallen-only.
6. Changed the toggle label to offer the opposite view:
   `Show Fallen Countries` from active-only, and `Show Active Countries` from
   fallen-only.
7. Reset the country-list scroll and clear a selected country if it no longer
   matches the chosen active/fallen filter.

## Files In Scope

- `src/core/version.h`
- `src/game/game_vassal_actions.c`
- `src/render/panel_country.c`
- `src/ui/ui.c`
- `src/ui/ui_debug_input.c`
- `src/ui/ui_wheel.c`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.2.a_side_doc.md`

## Behavioral Notes

- Vassal Release, Annex, and selected-country Independence should not force
  the simulation into pause.
- The vassal name-cell select/locate behavior from Ver0.3.2 remains unchanged.
- The country list now shows active countries only by default.
- Clicking `Show Fallen Countries` switches to fallen countries only.
- Clicking `Show Active Countries` switches back to active countries only.
- The World and Debug panel wheel paths preserve their existing content and
  only change input cadence.
- No world generation, plague, war, population, route, map-marker threshold,
  `MAX_CIVS`, or diplomacy balance changes were intended.

## Validation

- `WORLD_SIM_VERSION` is `0.3.2.a`.
- Canonical `make -B world_sim.exe` was attempted first and reached the link
  step, but the running canonical executable was locked by the operating
  system.
- `make -B TARGET=tmp_worldsim_ver032a_verify.exe` succeeded.
- The temporary executable contained `World Sim Game Ver 0.3.2.a`,
  `Show Active Countries`, and `Show Fallen Countries`, then was deleted.
- `build.bat` was attempted and reached the link step, but was blocked by the
  same locked canonical executable.
- Static and text checks were rerun before push; touched source/header files
  were at or below 500 lines.

## Residual Risks

- The root `world_sim.exe` could not be overwritten during release packaging
  because it was still running and locked.
- The pre-existing unrelated `src/sim/plague.c` line-count violation remains at
  515 lines; this hotfix did not touch plague or simulation logic.
- Full strict AGENTS Large-map regression was not rerun for this hotfix
  checkpoint.
- The release relies on focused source review, prior focused fixture evidence,
  and temporary-target build verification rather than a fresh live organic
  vassal GUI pass.
