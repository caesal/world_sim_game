# Ver0.3.5.c Side Doc

Ver0.3.5.c is a focused ocean-decoration spacing and collapsed-top-bar layout
checkpoint over Ver0.3.5.b.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.5.c`.
2. Strengthened ocean motif placement to prevent visible overlaps after the
   Ver0.3.5.b motif size increase.
3. Preserved the existing motif visual scale rather than shrinking assets.
4. Added footprint-aware exterior collision checks with inflated scaled
   manifest footprints.
5. Added footprint-aware interior tile AABB checks derived from scaled motif
   footprints and map dimensions.
6. Increased ocean motif spacing:
   - Exterior same-type spacing: `1180` normalized units.
   - Exterior different-type spacing: `680` normalized units.
   - Interior same-type spacing: `18` tiles.
   - Interior different-type spacing: `9` tiles.
7. Added focused ocean overlap regression artifacts and probe counters for
   spacing violations.
8. Fixed the collapsed top-bar layout so `Reset` / `重置` stays visible when the
   side panel is collapsed or the top bar is narrow.
9. Reserved the top-bar map-mode button lane against the Reset button rather
   than only the language button.
10. Drew Reset and Language after map-mode buttons so they remain visually on
    top.
11. Added top-bar layout probe artifacts for expanded/collapsed English and
    Chinese states.

## Files In Scope

- `Makefile`
- `build.bat`
- `src/core/version.h`
- `src/game/game_presentation_map_probe.c`
- `src/game/game_presentation_probe.c`
- `src/game/game_presentation_topbar_probe.c`
- `src/render/panel_info.c`
- `src/render/render_ocean_decoration.c`
- `src/render/render_ocean_decoration.h`
- `src/render/render_ocean_decoration_rules.c`
- `src/ui/ui_layout.c`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.5.c_side_doc.md`

## Behavioral Notes

- `MAP_SAVE_VERSION` remains `18`.
- Ocean decoration remains presentation-only.
- The ocean fix intentionally trades some decoration density for no visible
  overlap at the current motif size.
- The top-bar fix preserves existing controls and behavior; it only changes
  layout reservation and draw order.
- This checkpoint does not intentionally change gameplay, simulation,
  worldgen, diplomacy, war, alliances, population, plague, routes, resources,
  map fill semantics, save data, or versioned save compatibility.

## Validation Notes

- `git diff --check` passed.
- `make check-text` passed.
- `.c` include scan found no source file including another `.c` file.
- Touched/new `.c/.h` line counts were at or under the 500-line limit.
- Focused probes passed for presentation and worldgen surfaces.
- Key focused evidence included:
  - `case=ocean_decoration_layer ok=1`
  - `overlaps=0`
  - `spacing_violations=0`
  - `exterior_spacing=1`
  - `asset=1`
  - `motif_asset=1`
  - `primitive_waves=0`
  - `case=topbar_reset_layout ok=1`
- Visual artifacts were generated for:
  - `ocean_decoration_overlap_regression.bmp`
  - `ocean_decoration_full.bmp`
  - `ocean_decoration_zoom_pan.bmp`
  - `topbar_collapsed_en.bmp`
  - `topbar_collapsed_zh.bmp`
  - `topbar_expanded_en.bmp`
  - `topbar_expanded_zh.bmp`
- Full AGENTS Rule39 validation was not completed for Ver0.3.5.c. This
  checkpoint must not be treated as full release-ready gameplay acceptance
  until a fresh Rule39 run records final year/month, natural region count,
  civilization count, speed setting, five technology-stage-5 civilizations, and
  deep-sea hidden-before/revealed-after evidence.

## Residual Risks

- Focused probes cover the changed presentation surfaces but do not replace a
  live long-run Rule39 regression.
- Live GUI review remains useful for final visual acceptance of ocean motif
  density and top-bar spacing at unusual window widths.
