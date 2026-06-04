# Ver0.3.2 Side Doc

Ver0.3.2 is a vassal-action and map-marker checkpoint over Ver0.3.1.c. It
preserves the collapse-partition, interaction, plague-cooldown, fragmentation,
route, marker, and war-cadence work while adding direct-vassal overview actions
and clearer political-map marker documentation in the map legend.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.2`.
2. Added `src/game/game_vassal_actions.c`.
3. Updated `Makefile` and `build.bat` for the new vassal action module.
4. Kept `MAX_CIVS` at 200.
5. Added compact direct-vassal overview rows with:
   - colored clickable vassal name cell
   - Release / `释放`
   - Annex / `吞并`
6. Made the name cell select and locate the vassal without entering the
   modal/action pause path.
7. Added direct-vassal annex request handling that retires the vassal and
   transfers owned region/city ownership through the existing region-claim
   paths.
8. Updated ordinary city marker stages:
   - below 800: Outpost
   - 800-5399: Village
   - 5400-9499: Town
   - 9500 and above: City
9. Kept capital markers as the first override.
10. Slightly thickened lightweight GDI harbor glyph strokes without restoring
    PNG icon decoding.
11. Added a political-map legend glyph column with icon + name only.
12. Kept route-potential legend output route-only.

## Behavioral Notes

- Vassal name cells use the snapshot/live country color and choose dark or
  light text based on perceived luminance.
- Name-cell selection returns before `game_pause_for_modal_or_action()`.
- Release uses the existing release path.
- Annex is limited to direct vassals of the selected country.
- The political legend lists Outpost, Village, Town, City, Capital, Harbor, and
  Harbor Capital without showing thresholds.
- The route-potential legend lists only shallow and deep route entries.
- No world generation, plague, war, population, route-potential rules, or
  `MAX_CIVS` balance changes were intended.

## Validation

- `WORLD_SIM_VERSION` is `0.3.2`.
- Canonical `make -B world_sim.exe` and `build.bat` were attempted for the
  release; both reached the link step and were blocked by a locked
  `world_sim.exe`.
- A temporary-target release verification build succeeded and the temporary
  executable was deleted.
- Static and text checks were rerun for the release.
- Focused GUI evidence covered:
  - political legend glyph column in Chinese
  - route-potential route-only legend in Chinese
  - marker zoom display
  - legend collapse and expand behavior
- Focused vassal probes covered:
  - row hit targets for Select, Release, and Annex
  - bright and dark name-cell text contrast
  - Release clearing the overlord
  - Annex retiring the vassal and transferring region/city ownership
- The user confirmed the focused-only validation and approved this checkpoint
  for push.

## Residual Risks

- Full strict AGENTS Large-map regression was not rerun for this focused
  vassal/UI/map-display checkpoint.
- Live organic vassal GUI coverage was represented by probes/fixtures rather
  than a naturally occurring in-game vassal case.
- Future vassal annex work should continue to verify auto-annex semantics,
  region claim failure handling, diplomacy cleanup, and snapshot refresh
  together.
