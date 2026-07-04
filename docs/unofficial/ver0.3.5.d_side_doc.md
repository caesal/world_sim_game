# Ver0.3.5.d Side Doc

Ver0.3.5.d is a focused map-presentation, live-rendering, cache, and
diplomacy-arrow checkpoint over Ver0.3.5.c.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.5.d`.
2. Added a shared ocean texture cache so interior map water and exterior ocean
   draw from the same wave texture source.
3. Preserved separate interior/exterior motif placement while removing the
   visible hard rectangular seam between masked map water and exterior ocean.
4. Kept the collapsed map viewport full width while making map legend placement
   safe around the collapsed side-panel handle.
5. Tightened static map and static scene cache presentation so live political
   fill, province/city state, border-safe status, and city overlays stay
   current without requiring map-mode switches.
6. Reduced expensive max-speed static-scene churn while preserving live
   province/city/border correctness and existing map draw order.
7. Hardened cached full-window and deferred paint compatibility checks so stale
   map modes, stale legends, stale side-panel state, and stale language frames
   cannot flash back into the GUI.
8. Fixed map legend cleanup and sizing regressions, including stale/double
   legend footprints and empty Alliance legend artifacts.
9. Fixed diplomacy map-arrow presentation for new contact, peace, tension, and
   war events when real GUI paint paths take UI-only, cached, or deferred routes.
10. Added diagnostics for diplomacy active, enqueued, drawn, expired, and
    cached-paint-blocked arrow counters in the Debug / Performance panel.
11. Added focused presentation probes for live province/city refresh, layout,
    diplomacy-arrow transitions, cached-paint arrow guards, map-mode switching,
    ocean seam behavior, and border-safe presentation.
12. Added AGENTS guardrails that forbid performance/rendering fixes from
    suppressing real-time province, city, border, highlight, route, or diplomacy
    presentation correctness.

## Files In Scope

- `AGENTS.md`
- `Makefile`
- `build.bat`
- `src/core/profiler.c`
- `src/core/profiler.h`
- `src/core/version.h`
- `src/game/game_loop.c`
- `src/game/game_presentation_diplomacy_probe.c`
- `src/game/game_presentation_layout_probe.c`
- `src/game/game_presentation_map_probe.c`
- `src/game/game_presentation_probe.c`
- `src/game/game_presentation_regression_probe.c`
- `src/render/diplomacy_map_anim.c`
- `src/render/diplomacy_map_anim.h`
- `src/render/map_display_policy.c`
- `src/render/map_ownership_surface.c`
- `src/render/map_ownership_surface.h`
- `src/render/panel_debug_perf.c`
- `src/render/panel_map.c`
- `src/render/render.c`
- `src/render/render_ocean_decoration.c`
- `src/render/render_ocean_texture.c`
- `src/render/render_ocean_texture.h`
- `src/render/render_static_map_cache.c`
- `src/render/render_static_map_cache_fill.c`
- `src/render/render_static_scene.c`
- `src/render/render_static_scene.h`
- `src/sim/simulation_worker.c`
- `src/ui/ui.c`
- `src/ui/ui_layout.c`
- `src/ui/ui_map_display.c`
- `src/ui/ui_map_display.h`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.5.d_side_doc.md`

## Behavioral Notes

- `MAP_SAVE_VERSION` remains `18`.
- The release is scoped to presentation, render caching, diagnostics,
  validation probes, and repository instructions.
- No gameplay, diplomacy/contact rules, war rules, world generation, route
  unlock rules, speed semantics, balance values, save schema, plague,
  population, economy, or resource simulation rules are intentionally changed.
- Diplomacy arrows still use structured event-log entries and RenderSnapshot
  presentation data. The fix changes dynamic overlay invalidation and cached
  paint eligibility, not the simulation event rules.
- Performance and cache changes are constrained by AGENTS rules 44-46: political
  ownership, province fill, borders, city icons, city labels, routes, highlights,
  and diplomacy arrows must remain live.

## Validation Notes

- `make -B world_sim.exe` passed for this release push.
- `cmd /c build.bat` passed for this release push.
- `git diff --check` passed, with line-ending warnings only where reported.
- `make check-text` passed.
- `python tools/check_mojibake.py` passed.
- `.c` include scan found no source file including another `.c` file.
- Touched/new `.c/.h` line counts were at or under the 500-line limit.
- Hidden `world_sim.exe --probe-presentation` passed with `overall_ok=1`.
- Key focused probe evidence included:
  - `case=live_province_city_update ok=1`
  - `case=map_layout_legend_edge ok=1`
  - `case=ocean_decoration_layer ok=1`
  - `case=diplomacy_transition_no_contact_peace ok=1`
  - `case=diplomacy_transition_peace_to_tense ok=1`
  - `case=diplomacy_transition_war_start ok=1`
  - `case=diplomacy_cached_paint_guard ok=1`
- User live-check acceptance was provided for the scoped rendering and
  diplomacy-arrow fixes before this release push.

## Residual Risks

- Full AGENTS Rule39 validation was not completed for Ver0.3.5.d. This
  checkpoint must not be treated as full release-ready gameplay acceptance until
  a fresh Rule39 run records final year/month, natural region count,
  civilization count, speed setting, five technology-stage-5 civilizations,
  deep-sea hidden-before/revealed-after evidence, and performance evidence.
- The release includes multiple render-cache and UI presentation paths. Future
  fixes should avoid adding more code to files already at the 500-line limit and
  should split responsibilities before further expansion.
