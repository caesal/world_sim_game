# Ver0.3.3.f Side Doc

Ver0.3.3.f is a broad alliance, diplomacy, map-display, and presentation
performance checkpoint over Ver0.3.3.e. It promotes alliances from pairwise
diplomacy display into named alliance entities, adds the Alliance map view, and
stabilizes the rendering path after several Extreme-map presentation
regressions.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.3.f`.
2. Added formal alliance entities with names, member tracking, founder/color,
   joining, leaving, kick/cooldown behavior, snapshot support, and save/load
   support.
3. Added the Alliance map display mode, alliance labels, alliance coloring,
   alliance-aware selected-country highlight behavior, and alliance legend
   support.
4. Added player country actions for Declare War, Peace, Vassalize, Alliance,
   and Leave Alliance while preserving existing war, truce, and vassal rules.
5. Added cached directional diplomacy relation scores and hover factor
   explanations for diplomacy relation cards.
6. Added ordered completed-month presentation and Debug / Performance counters
   for pending, shown, dropped, skipped, and render spike attribution.
7. Fixed static political fill lag where city icons, labels, and borders could
   advance while Country/Alliance/All fill reused an old static cache.
8. Optimized Extreme-map 5x rendering by profiling render subphases and
   reducing political/province fill and boundary work in heavy display modes.
9. Fixed setup random-button first-click repetition, manual color-lock behavior,
   and post-expansion adjacent-country color repair.
10. Updated the root README, documentation index, version log, this side doc,
    active version marker, and in-game pause-menu version summary.

## Files In Scope

- `AGENTS.md`
- `Makefile`
- `build.bat`
- `src/core/version.h`
- `src/core/constants.h`
- `src/core/dirty_flags.*`
- `src/core/profiler.*`
- `src/core/render_snapshot*`
- `src/game/*diplomacy*probe*`
- `src/game/game_alliance_probe.*`
- `src/game/game_alliance_render_probe.*`
- `src/game/game_loop.*`
- `src/game/game_player_actions.*`
- `src/game/game_presentation_probe.*`
- `src/io/map_save*`
- `src/render/map_display_policy.*`
- `src/render/map_ownership_surface.*`
- `src/render/map_label_alliance.*`
- `src/render/map_label_cache.c`
- `src/render/panel_debug*`
- `src/render/render*`
- `src/render/snapshot_map_layers.c`
- `src/render/terrain_present.c`
- `src/sim/alliance*`
- `src/sim/civ_color_repair.c`
- `src/sim/civ_colors.*`
- `src/sim/diplomacy*`
- `src/sim/regions_spawn.c`
- `src/sim/simulation*`
- `src/ui/color_picker.c`
- `src/ui/ui*`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.3.f_side_doc.md`

## Behavioral Notes

- `MAP_SAVE_VERSION` is now `14` to include alliance save state.
- Alliance names are generated from the provided alliance-name pool and reused
  with Roman-numeral suffixes after prior names are exhausted.
- Vassals follow their overlord for alliance display/defense scope and do not
  vote as sovereign alliance members.
- Player alliance commands are intentionally forceful and bypass AI waiting and
  voting gates.
- Completed months are displayed in order and must not be dropped or replaced
  by fake top-bar dates.
- Render profiling is diagnostic/presentation infrastructure; it does not
  change simulation rules.
- Map ownership/fill semantics are intended to remain unchanged by the final
  performance pass; the changes reduce cache churn and fill computation cost.

## Validation

Release validation for this commit should record:

- Canonical `make -B world_sim.exe`, or the AGENTS temporary-target workflow if
  a running validation executable locks the canonical output.
- `cmd /c build.bat`, or the AGENTS temporary-output workflow if locked.
- `git diff --check`.
- `.c` include scan confirming no source file includes another `.c` file.
- `make check-text`.
- Touched `.c/.h` line counts under the 500-line limit.
- Touched-file mojibake marker scan.
- `world_sim.exe --probe-presentation`.
- `world_sim.exe --probe-diplomacy`.
- Root executable inventory confirming the repository root only contains
  `world_sim.exe`.

Focused evidence from the final render-mode stutter pass is recorded under:

- `build/validation/render_mode_stutter_afterfix4_20260619_173457/`

Full Rule39 evidence from the final pass is recorded under:

- `build/validation/rule39_render_mode_20260619_175053/`

Recorded Rule39 summary:

- Final year/month: `1461/4`.
- Natural regions: `1189`.
- Civilization slots: `46`.
- Alive civilizations: `38`.
- Speed: max/5x.
- Five stage-5+ civilizations:
  - `0 Dragonwatch Realm stage 10`
  - `1 Sunfall Empire stage 10`
  - `2 Goldenreach League stage 10`
  - `3 Dragonmere Empire stage 10`
  - `4 Brightspire Republic stage 10`
- Deep-sea route evidence: before unlock deep `0`; after unlock routes `88`,
  shallow `66`, deep `22`.

## Residual Risks

- The release contains a large accumulated feature stack. Future work should
  favor smaller commits or intermediate checkpoints to keep regression causes
  easier to isolate.
- Province view remains one of the heavier display modes, though focused and
  Rule39 evidence show it no longer blocks 5x presentation in the same way as
  the pre-fix Alliance/Country path.
- The final render optimization touched `map_display_policy.*` and
  `map_ownership_surface.*` for cache/read-model optimization. Future work
  should avoid broad semantic edits in those modules without live evidence.
