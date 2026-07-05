# Ver0.3.5.e Side Doc

Ver0.3.5.e is a focused UI and map-presentation checkpoint over Ver0.3.5.d.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.5.e`.
2. Added Alliance-view color swatch hit handling in Alliance list and detail
   views.
3. Reused the existing civilization color picker so editing an Alliance-view
   swatch edits the alliance leader civilization color.
4. Kept leader civilization color as the single alliance-color source of truth;
   no independent persistent alliance color was added.
5. Updated Alliance panel/model and Alliance map fill color resolution so the
   displayed alliance color follows the current leader civilization color.
6. Changed map legend panel opacity to 128/255, approximately 50 percent.
7. Changed the shared ocean texture base color to `RGB(55, 135, 199)`.
8. Made map viewport blank-space clicks clear selection while preserving normal
   real-map tile and side-panel click behavior.
9. Allowed cached/deferred presentation paths to draw diplomacy animations as
   a dynamic overlay instead of blocking compatible cached paints during
   diplomacy animation bursts.
10. Added focused presentation probes for Alliance color swatch hits,
    viewport-blank click geometry, expanded diplomacy burst handling, and
    cached-paint diplomacy animation behavior.

## Files In Scope

- `src/core/version.h`
- `src/game/game_presentation_probe.c`
- `src/game/game_presentation_regression_probe.c`
- `src/render/map_display_policy.c`
- `src/render/panel_alliance.c`
- `src/render/panel_alliance.h`
- `src/render/panel_alliance_model.c`
- `src/render/panel_map.c`
- `src/render/render.c`
- `src/render/render_ocean_texture.c`
- `src/sim/alliance_query.c`
- `src/ui/ui_alliance_panel_input.c`
- `src/ui/ui_map_input.c`
- `src/ui/ui_map_input.h`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.5.e_side_doc.md`

## Behavioral Notes

- `MAP_SAVE_VERSION` remains `18`.
- No gameplay, diplomacy/contact rules, war rules, world generation, route
  unlock rules, speed semantics, balance values, save schema, plague,
  population, economy, or resource simulation rules are intentionally changed.
- Alliance color remains leader-civilization color. The Alliance-view swatch is
  an additional UI entry point to the same leader civilization color picker.
- The map legend opacity and ocean base color are presentation-only changes.
- The viewport blank-click change affects selection clearing only; it does not
  change map ownership, terrain, simulation, or panel rules.
- The diplomacy cached-paint change preserves arrow visibility while reducing
  unnecessary full presentation blocking during animation bursts.

## Validation Notes

- Canonical build, batch build, focused presentation probe, text hygiene, source
  include scan, and line-count validation are required for this checkpoint.
- Key focused probe evidence should include:
  - `case=alliance_panel_model ok=1`
  - `case=diplomacy_transition_burst ok=1`
  - `case=diplomacy_cached_paint_guard ok=1`
  - `case=map_viewport_blank_click ok=1`
  - `overall_ok=1`

## Residual Risks

- Full AGENTS Rule39 validation was not completed for Ver0.3.5.e. This
  checkpoint must not be treated as full release-ready gameplay acceptance until
  a fresh Rule39 run records final year/month, natural region count,
  civilization count, speed setting, five technology-stage-5 civilizations,
  deep-sea hidden-before/revealed-after evidence, realtime province/city
  correctness, and performance evidence.
- The user-reported initial 0-25 year stutter remains unresolved and should be
  diagnosed from this pushed checkpoint before the next Software Engineer
  implementation pass.
