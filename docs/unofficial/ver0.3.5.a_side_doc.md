# Ver0.3.5.a Side Doc

Ver0.3.5.a is a focused union, presentation, event-log, and collapse-color
checkpoint over Ver0.3.5.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.5.a`.
2. Reworked alliance union so an eligible proposer absorbs the other formal
   members instead of creating a separate new union civilization.
3. Added council-style union proposal timing, proposer cooldown, and weighted
   strict 3/4 union voting.
4. Preserved proposer identity on successful union: name, color, capital,
   diplomacy, and current wars remain with the proposer.
5. Ended absorbed members as fallen/absorbed members and folded their owned
   state into the proposer without automatically inheriting their external
   wars.
6. Added global union-completion event-log visibility for Debug/Event Log views
   and country recent-event cards.
7. Improved civil-unrest and enclave successor color selection so newly created
   countries avoid parent, border-neighbor, and sibling-successor colors.
8. Polished speed/status presentation with a compact map-viewport `Nms` badge
   and chip-style bottom Render/Queue/Status display.
9. Preserved visible Queue values in English and Chinese, including `0`, `99`,
   and `999`.
10. Made Alliance Votes relation bars passive hover-only tooltip targets so
    click or mouse-down does not create button-like behavior.
11. Updated build lists and focused probes for the new validation surfaces.

## Files In Scope

- `Makefile`
- `build.bat`
- `src/core/event_log.c`
- `src/core/game_types.h`
- `src/core/version.h`
- `src/game/game.h`
- `src/game/game_alliance_lifecycle_probe.c`
- `src/game/game_alliance_record_probe.c`
- `src/game/game_collapse_color_probe.c`
- `src/game/game_collapse_color_probe.h`
- `src/game/game_military_alliance_probe.c`
- `src/game/game_military_alliance_rules_probe.c`
- `src/game/game_player_actions.c`
- `src/game/game_presentation_map_probe.c`
- `src/game/game_presentation_probe.c`
- `src/io/map_save.c`
- `src/io/map_save_state.c`
- `src/main.c`
- `src/render/panel_alliance_council.c`
- `src/render/panel_alliance_council.h`
- `src/render/panel_alliance_detail.c`
- `src/render/panel_alliance_history.c`
- `src/render/panel_alliance_sections.c`
- `src/render/panel_alliance_vote_state.c`
- `src/render/panel_alliance_votes.c`
- `src/render/panel_country_events.c`
- `src/render/panel_debug.c`
- `src/render/panel_debug.h`
- `src/render/panel_map.c`
- `src/render/panel_map_speed_badge.c`
- `src/render/panel_map_speed_badge.h`
- `src/render/render.c`
- `src/sim/alliance.h`
- `src/sim/alliance_council.c`
- `src/sim/alliance_query.c`
- `src/sim/alliance_records.c`
- `src/sim/alliance_state.c`
- `src/sim/alliance_union.c`
- `src/sim/civ_colors.c`
- `src/sim/civ_colors.h`
- `src/sim/collapse.c`
- `src/sim/enclave_resolution.c`
- `src/ui/ui_alliance_panel_input.c`
- `src/ui/ui_alliance_panel_input.h`
- `src/ui/ui_invalidation.c`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.5.a_side_doc.md`

## Behavioral Notes

- `MAP_SAVE_VERSION` is now `18` because alliance state persists council
  previous-vote and vote-snapshot data used by current alliance records.
- Defensive Alliance union eligibility remains 800 years.
- Military Alliance union eligibility remains 500 years.
- Union proposals use council-style voting and require strict approval above
  the 3/4 threshold.
- The proposer keeps its existing civilization identity after a successful
  union.
- Absorbed members do not pass their external wars to the proposer.
- Alliance Votes relation bars remain tooltip targets, not clickable controls.
- No map ownership/fill, world-generation, route-potential, plague,
  population, economy, or province-border semantics are intentionally changed
  by this checkpoint.

## Validation Notes

- Canonical `make -B world_sim.exe` passed.
- Canonical `cmd /c build.bat` passed.
- `git diff --check` passed with line-ending warnings only.
- `make check-text` passed.
- `.c` include scan found no source file including another `.c` file.
- Touched/new `.c/.h` line counts were at or under the 500-line limit.
- `world_sim.exe --probe-presentation` passed with `overall_ok=1`.
- `world_sim.exe --probe-diplomacy` passed with `overall_ok=1`.
- `world_sim.exe --probe-military-alliance` passed.
- `world_sim.exe --probe-collapse-colors` passed.
- `world_sim.exe --probe-worldgen` passed.
- Focused evidence included:
  - `case=map_speed_status ok=1`
  - `case=alliance_union_vote_absorption ok=1`
  - `case=union_proposer_timing ok=1`
  - collapse-color probe rows showing parent, border-neighbor, and
    sibling-successor color distance checks.
- Full AGENTS Rule39 validation was not completed for Ver0.3.5.a. This
  checkpoint must not be treated as full release-ready gameplay acceptance
  until a fresh Rule39 run records final year/month, natural region count,
  civilization count, speed setting, five technology-stage-5 civilizations, and
  deep-sea hidden-before/revealed-after evidence.

## Residual Risks

- Focused probes cover the changed systems but do not replace a live long-run
  Rule39 regression.
- Live GUI inspection remains useful for naturally occurring union votes,
  collapse outcomes, speed UI placement, and event-log filters before broad
  release-ready claims.
