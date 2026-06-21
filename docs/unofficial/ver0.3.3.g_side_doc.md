# Ver0.3.3.g Side Doc

Ver0.3.3.g is a focused Alliance UI, alliance records, and release metadata
checkpoint over Ver0.3.3.f. It keeps the Ver0.3.3.f alliance entity, map
display, and render-performance base, then turns alliances into a first-class
right-panel experience with list/detail routing and readable records.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.3.g`.
2. Added first-class Alliance right-panel routing: the first tab becomes
   Alliance only in Alliance map view, while other map views keep Country.
3. Moved map-view controls into the top toolbar with six visible modes:
   Country, Alliance, Geography, Climate, Province, and Routes.
4. Added Alliance list rows, no-alliance country rows, extinct-country toggle
   support, and Country-style sorting behavior in Alliance view.
5. Added Alliance Detail sections for Overview, Members, Votes, History, and
   Union progress.
6. Added persistent bounded alliance candidate, vote, and history records with
   save/load support.
7. Updated Alliance Overview cards and fixed aspect-preserving icon rendering.
8. Reworked Alliance Members into compact Country-list-style rows with
   Population, Provinces, Army, Technology, and Joined year columns.
9. Reworked Alliance Votes and History into readable log-card style panels;
   vote cards show each stored member vote directly as subrows.
10. Updated the root README, documentation index, version log, this side doc,
    active version marker, and in-game pause-menu version summary.

## Files In Scope

- `Makefile`
- `build.bat`
- `src/core/constants.h`
- `src/core/game_types.h`
- `src/core/version.h`
- `src/game/game_alliance_probe.c`
- `src/game/game_alliance_record_probe.*`
- `src/game/game_presentation_probe.c`
- `src/io/map_save*`
- `src/render/icons.*`
- `src/render/panel_alliance*`
- `src/render/panel_debug.c`
- `src/render/panel_info.c`
- `src/render/panel_map.c`
- `src/render/render_panel_internal.h`
- `src/sim/alliance*`
- `src/ui/pause_menu.c`
- `src/ui/ui*`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.3.g_side_doc.md`

## Behavioral Notes

- `MAP_SAVE_VERSION` is now `15` because alliance candidate, vote, and history
  records are serialized.
- Alliance vote and history records are bounded: each alliance keeps the most
  recent 32 vote records and 128 history events.
- Existing alliance AI thresholds, voting rules, war rules, diplomacy scoring,
  vassal rules, and map ownership/fill semantics are not changed by this
  release metadata pass.
- The Union panel is display-only in this checkpoint; it does not merge
  alliance members into a single country.

## Validation

Release validation for this checkpoint included:

- Canonical `make -B world_sim.exe` was attempted first and failed only because
  a running `world_sim.exe` locked the output file.
- AGENTS temporary-target `make -B TARGET=world_sim_validation_tmp.exe` passed
  and the temporary executable was deleted.
- Canonical `cmd /c build.bat` was attempted first and failed only because the
  same running `world_sim.exe` locked the output file.
- A temporary `build.bat` output-name verification passed and temporary files
  were deleted.
- `git diff --check` passed with only line-ending warnings.
- `.c` include scan found no source file including another `.c` file.
- `make check-text` passed.
- Touched `.c/.h` line counts were under the 500-line limit.
- Touched-file mojibake marker scan passed after the pause-menu mojibake text
  was corrected.
- `world_sim.exe --probe-presentation` passed with `overall_ok=1`.
- `world_sim.exe --probe-diplomacy` passed with `overall_ok=1`.
- Root executable inventory contained exactly `world_sim.exe`.

Full Rule39 evidence from the Alliance UI polish pass is recorded under:

- `build/validation/alliance_ui_polish_20260620_230000/`

Recorded Rule39 summary:

- Final year/month: `733/1`.
- Natural regions: `1115`.
- Confirmed civilization count: `35`.
- Speed: max/5x.
- Five stage-5 civilizations:
  - `0 Realm of Veyr stage 5`
  - `1 ZhuNing High Kingdom stage 5`
  - `2 Eastern GaramHan Court stage 5`
  - `3 Celestial LuoYan Realm stage 5`
  - `4 HaneulGuang High Kingdom stage 5`
- Deep-sea route evidence: before unlock `0/0/0`; after unlock routes `53`,
  shallow `49`, deep `4`.

## Residual Risks

- This release contains a large UI/data stack. Future Alliance UI work should
  prefer smaller incremental commits so regressions can be isolated faster.
- The version metadata correction is intentionally committed after the initial
  Alliance UI push. The `ver0.3.3.g` tag must point at the final metadata commit,
  not the earlier UI-only commit.
