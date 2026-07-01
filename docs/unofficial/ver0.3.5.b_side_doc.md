# Ver0.3.5.b Side Doc

Ver0.3.5.b is a focused Alliance Detail action, ancient-ocean presentation, and
decision-countdown refresh checkpoint over Ver0.3.5.a.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.5.b`.
2. Added Alliance Detail Overview buttons under the Council for direct player
   `Invite Join` and `Remove Member` actions.
3. Added alliance invite/remove target-arrow modes with target-specific invalid
   messages for already-allied invite targets and non-member remove targets.
4. Reused alliance member mutation, council recalculation, structured history,
   structured events, dirty flags, and snapshot publication for successful
   player direct alliance actions.
5. Added the ocean decoration asset library under
   `assets/ocean_decoration_reference/`.
6. Added runtime manifest-driven transparent PNG motif loading from
   `assets/ocean_decoration_reference/motifs/`.
7. Expanded motif runtime capacity to support the current 27 resource motifs.
8. Added and tuned antique ocean texture rendering so exterior ocean and
   interior water can share the same chart-like sea texture.
9. Tuned ocean visuals after review: bluer water and motif display size at
   `6/5` of the prior runtime display size.
10. Fixed Decision panel countdown freshness by refreshing countdown fields when
    cached decision snapshots are reused and by including those countdowns in
    the panel cache key.
11. Updated focused probes for alliance player actions, ocean decoration,
    countdown refresh, and presentation validation.

## Files In Scope

- `Makefile`
- `build.bat`
- `assets/ocean_decoration_reference/`
- `src/core/render_snapshot_civs.c`
- `src/core/version.h`
- `src/game/game_alliance_render_probe.c`
- `src/game/game_military_alliance_rules_probe.c`
- `src/game/game_player_actions.c`
- `src/game/game_player_actions.h`
- `src/game/game_presentation_map_probe.c`
- `src/game/game_presentation_probe.c`
- `src/render/country_target_arrow.c`
- `src/render/panel_alliance_detail.c`
- `src/render/panel_alliance_detail.h`
- `src/render/panel_map.c`
- `src/render/panel_view_model_cache.c`
- `src/render/panel_view_model_cache.h`
- `src/render/render_ocean_assets.c`
- `src/render/render_ocean_assets.h`
- `src/render/render_ocean_decoration.c`
- `src/render/render_ocean_decoration.h`
- `src/render/render_ocean_decoration_rules.c`
- `src/render/render_ocean_decoration_rules.h`
- `src/render/render_ocean_decoration_water.c`
- `src/render/render_ocean_decoration_water.h`
- `src/render/render_ocean_motifs.c`
- `src/render/render_ocean_motifs.h`
- `src/render/render_static_map_cache.c`
- `src/render/render_static_scene.c`
- `src/sim/alliance.h`
- `src/sim/alliance_state.c`
- `src/sim/decision_snapshot.c`
- `src/sim/decision_snapshot.h`
- `src/ui/ui_alliance_panel_input.c`
- `src/ui/ui_country_target.c`
- `src/ui/ui_country_target.h`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.5.b_side_doc.md`

## Behavioral Notes

- `MAP_SAVE_VERSION` remains `18`.
- Player direct alliance invite/remove is intentionally separate from AI
  candidate and vote flows.
- Direct alliance invite/remove remains a player command path. It is not an AI
  diplomacy balance change.
- Ocean decoration is presentation-only. It does not intentionally alter
  world generation, terrain, water depth, ownership, political fill, province
  borders, city icons, route potential, diplomacy, war, economy, population,
  plague, or save data.
- Decision countdown refresh changes presentation freshness, not the underlying
  decision timers or simulation schedule.

## Validation Notes

- Canonical `make -B world_sim.exe` was attempted first.
- The canonical executable was locked by a running `world_sim.exe`, so
  temporary-target build verification was used as allowed by AGENTS.
- `git diff --check` passed.
- `make check-text` passed.
- `.c` include scan found no source file including another `.c` file.
- Touched/new `.c/.h` line counts were at or under the 500-line limit.
- Focused probes passed for presentation, diplomacy, military-alliance, and
  worldgen surfaces.
- Key focused evidence included:
  - `case=ocean_decoration_layer ok=1`
  - `asset=1`
  - `motif_asset=1`
  - `primitive_waves=0`
  - `overlaps=0`
  - `case=decision_countdown_refresh ok=1`
  - `case=player_direct_alliance_actions ok=1`
- Full AGENTS Rule39 validation was not completed for Ver0.3.5.b. This
  checkpoint must not be treated as full release-ready gameplay acceptance
  until a fresh Rule39 run records final year/month, natural region count,
  civilization count, speed setting, five technology-stage-5 civilizations, and
  deep-sea hidden-before/revealed-after evidence.

## Residual Risks

- Focused probes cover the changed systems but do not replace a live long-run
  Rule39 regression.
- Live GUI inspection remains useful for final visual acceptance of ocean
  motif density, ocean color, player target arrows, and Alliance Detail action
  placement.
