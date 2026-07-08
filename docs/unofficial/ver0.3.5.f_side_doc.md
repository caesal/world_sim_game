# Ver0.3.5.f Side Doc

Ver0.3.5.f is a backup UI/rendering checkpoint over Ver0.3.5.e.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.5.f`.
2. Preserved the Ver0.3.5.e Alliance-view color editing, ocean color, legend
   opacity, viewport-blank click clearing, and diplomacy cached-paint work.
3. Restored `MAP_LAYER_CACHE_SCALE` to `2` so province and country border
   raster thickness matches the established map presentation.
4. Reworked cached/deferred full-window presentation paths to reduce stale
   top-bar, map, side-panel, and bottom-bar frame alternation.
5. Removed a synchronous pressed-state repaint path that could contribute to
   visible full-screen flicker.
6. Added cached diplomacy marker icon bitmaps while preserving contact/peace,
   tension, and war arrow presentation before Year 50.
7. Scoped the alliance Votes-tab diplomacy relationship tooltip so it does not
   appear on the Alliance Council overview.
8. Kept active alliance upgrade vote joiners visible as current alliance
   members while marking them as next-round voters instead of rewriting the
   existing vote cohort.
9. Added war comparison bar presentation for regular, mercenary, vassal, and
   alliance force segments, with explicit left/right regular-army guards.
10. Reduced live fill churn through the existing revision-keyed ownership
    surface fast path where safe.

## Files In Scope

- `src/core/version.h`
- `src/game/game_loop.c`
- `src/game/game_presentation_diplomacy_probe.c`
- `src/game/game_presentation_interaction_probe.c`
- `src/game/game_presentation_layout_probe.c`
- `src/game/game_presentation_map_probe.c`
- `src/game/game_presentation_regression_probe.c`
- `src/render/diplomacy_map_anim.c`
- `src/render/diplomacy_map_marker_cache.c`
- `src/render/diplomacy_map_marker_cache.h`
- `src/render/panel_alliance_detail.c`
- `src/render/panel_alliance_vote_state.c`
- `src/render/panel_alliance_votes.c`
- `src/render/panel_country_diplomacy_cards.c`
- `src/render/panel_map.c`
- `src/render/panel_war_compare_bar.c`
- `src/render/panel_war_compare_bar.h`
- `src/render/render.c`
- `src/render/render_ocean_decoration.c`
- `src/render/render_static_map_cache.c`
- `src/render/render_static_map_cache_fill.c`
- `src/render/render_static_scene.c`
- `src/sim/simulation_worker.c`
- `src/ui/ui.c`
- `src/ui/ui_alliance_panel_input.c`
- `src/ui/ui_layout.c`
- `src/ui/ui_pressed_state.c`
- `Makefile`
- `build.bat`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.5.f_side_doc.md`

## Behavioral Notes

- `MAP_SAVE_VERSION` remains `18`.
- No gameplay, diplomacy/contact rules, war rules, alliance rules, world
  generation, route unlock rules, speed semantics, balance values, save schema,
  plague, population, economy, or resource simulation rules are intentionally
  changed.
- `simulation_worker.c` changes are limited to idle-to-active timing baseline
  measurement and must not be treated as a speed-rule change.
- This is a backup checkpoint for the current acceptable presentation state,
  not a declaration that the early-year stutter is solved.

## Validation Notes

- Required release validation for this checkpoint includes canonical
  `make -B world_sim.exe`, `cmd /c build.bat`, `git diff --check`,
  `make check-text`, `python tools/check_mojibake.py`, `.c` include scan,
  touched `.c/.h` line-count checks, and focused `--probe-presentation`.
- Expected focused evidence includes:
  - `case=border_visual_scale_guard ok=1 scale=2 expected=2`
  - `case=no_fullscreen_flicker_cached_frame_guard ok=1`
  - `case=early_years_no_year_jump_presentation_guard ok=1`
  - `case=live_province_city_update ok=1`
  - `case=map_mode_switch_latency ok=1`
  - `case=alliance_council_no_diplomacy_tooltip ok=1`
  - `case=war_compare_regular_left_visible ok=1`
  - `case=war_compare_regular_right_visible ok=1`
  - `overall_ok=1`

## Residual Risks

- Full AGENTS Rule39 validation was not completed for Ver0.3.5.f.
- Targeted GUI performance evidence from the candidate stack still exceeded
  the requested hard gates for early-year actual ms/month and render ms.
- The initial 0-25 year stutter and real GUI mode-switch spikes remain the
  next performance diagnosis target after this backup checkpoint.
- Do not claim full release-ready/gameplay acceptance or performance acceptance
  until a fresh Rule39 run and the targeted performance gates pass.
