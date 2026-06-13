# Ver0.3.3.a Side Doc

Ver0.3.3.a is a scoped map-generation, map-presentation, and population-display
checkpoint over Ver0.3.3. It adds the Extreme map size requested after the
world-generation UI review, improves Extreme-map readability, and smooths a
display-only population-pyramid cliff between `65-69` and `70-74`.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.3.a`.
2. Added `MAP_SIZE_EXTREME`, raising `MAP_SIZE_COUNT` to four choices.
3. Set the map dimensions and natural-region caps to:
   - Small: `576x400`, cap `512`
   - Medium: `720x500`, cap `768`
   - Large: `864x600`, cap `1024`
   - Extreme: `1152x800`, cap `1536`
4. Updated world-generation layout, hit testing, and region estimates so the
   UI reflects the selected map size and cap.
5. Updated generation repair/headroom behavior so it uses the active map cap
   rather than the global maximum.
6. Changed the Physical random button to randomize each physical slider
   independently in `5..95`.
7. Changed the Advanced random button to randomize each advanced terrain
   preference slider and the natural-region-size slider independently in
   `5..95`.
8. Added a shared Extreme-map presentation policy for border widths.
9. Raised Extreme province borders to `2px` and country borders to `3px` across
   static cache, snapshot fallback, contour, and vector rendering paths.
10. Reduced Extreme city and port markers while preserving capital and harbor
    readability.
11. Smoothed only the display cache for ages `65..74`, preserving real
    male/female totals and simulation storage exactly.
12. Updated the pause-menu version summary, including clean UTF-8 Chinese text
    for this release note.

## Files In Scope

- `src/core/constants.h`
- `src/core/game_state.c`
- `src/core/version.h`
- `src/game/game_population_corner_probe.c`
- `src/render/cartography_layers.c`
- `src/render/contour_paths.c`
- `src/render/map_presentation_policy.h`
- `src/render/map_render.c`
- `src/render/panel_worldgen.c`
- `src/render/render_static_map_cache_border.c`
- `src/render/snapshot_map_layers.c`
- `src/render/vector_paths.c`
- `src/sim/population_display_cohorts.c`
- `src/sim/regions.c`
- `src/sim/regions.h`
- `src/sim/regions_config.c`
- `src/sim/regions_shape.c`
- `src/sim/regions_validate.c`
- `src/ui/pause_menu.c`
- `src/ui/ui_forms.c`
- `src/ui/ui_layout.c`
- `src/ui/ui_worldgen_layout.c`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.3.a_side_doc.md`

## Behavioral Notes

- Save format is unchanged.
- `map_size_index` and `pending_map_size` remain serialized as integer settings
  and clamp through the expanded `MAP_SIZE_COUNT`.
- Older saves continue to load through the same setting path.
- New Extreme worlds use the existing save layout but may exceed older binary
  dimension assumptions.
- The map presentation policy is dimensional: it applies the thicker border
  policy only when the active map dimensions match the Extreme maximum.
- Non-Extreme map border behavior is preserved.
- Population smoothing is presentation-only. It changes the display cache for
  the `65-69` and `70-74` rows but does not alter real cohorts, mortality,
  aging, migration, birth/death formulas, or save data.

## Validation

Focused implementation evidence from the Ver0.3.3.a work:

- `build/validation/map_size_extreme_20260612/map_size_probe_output.txt`
  confirmed all four map sizes and active caps:
  - Small generated `576x400`, regions `366/512`
  - Medium generated `720x500`, regions `532/768`
  - Large generated `864x600`, regions `718/1024`
  - Extreme generated `1152x800`, regions `1093/1536`
  - `result failures=0`
- `build/validation/population_display_65_74_20260612/summary.txt` confirmed
  male `1000`, female `800`, total `1800`, display `65-69=900`,
  display `70-74=900`, `cliff_delta=0`, and `failures=0`.
- GUI evidence under
  `build/validation/extreme_visual_readability_20260612/gui/` includes Extreme
  generation with 26 civilizations, selected/country highlight screenshots,
  the Population panel, and the Country Population subtab.
- Text/static/build evidence for this release commit is recorded in the final
  release response rather than in this side doc.

Strict AGENTS Rule39 game-flow regression was not completed for this pushed
checkpoint. This commit records focused map-size, GUI, and population-display
evidence, but it must not be treated as full Rule39 release-readiness evidence.

## Residual Risks

- Extreme maps increase tile count and natural-region capacity, so long-run
  performance and full game-flow behavior still need Rule39 validation.
- Focused screenshots can demonstrate readability, but they are not equivalent
  to real-screen flicker or stutter validation.
- Population display smoothing intentionally hides only a visual cliff; it does
  not rebalance elderly mortality or real cohort dynamics.
