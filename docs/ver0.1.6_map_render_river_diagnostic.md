# Ver0.1.6 Map Rendering And River Diagnostic

This diagnostic records the targeted polish pass requested before the next rendering/river cleanup.

## Findings

1. Border and coastline rendering lives in `src/render/border_paths.c`.
2. `draw_country_border_paths()` uses a dark outer stroke and colored inner stroke. The width is intended as screen pixels, then converted into the 2x vector layer source space.
3. `draw_province_border_paths()` draws province borders at `tile_size >= 4`, so province detail can appear early and add visual weight.
4. `draw_coastline_paths()` draws a dark outer coastline plus a green inner coastline. Together with `draw_coast_halo()`, this can look like a wall.
5. `draw_political_region_fills()` only runs in Political mode, but Political mode already tints ownership in `tile_display_color()`.
6. `draw_coast_halo()` blurs the land mask twice and runs in every map mode.
7. `draw_map_labels()` always draws country labels, while city labels start at medium zoom. Country labels use a heavy offset shadow, which can look muddy.
8. `draw_rivers()` draws an outline stroke and an inner stroke from explicit `RiverPath` objects. The current river width can reach 4 pixels plus outline.
9. `src/render/render.c` has a full-map layer cache, but any pan, zoom, display-mode change, side-panel width change, or world visual revision rebuilds the full map layer.
10. River mouths are `GEO_OCEAN`, `GEO_BAY`, or `GEO_LAKE`.
11. Current river acceptance already rejects pure dead-end rivers, but `trace_river_from()` can attach a nearby mouth within radius 5 after tracing stops, which can create an unnatural final jump.
12. No `.c` or `.h` file exceeded 500 lines during this diagnostic.

## Targeted Follow-Up Plan

1. In `border_paths.c`, reduce country/coast alpha and stroke widths, raise the province-border zoom gate, and reduce coast halo strength.
2. In `map_labels.c`, make country label shadows lighter, gate labels by zoom and territory size, and show fewer city labels at medium zoom.
3. In `map_render.c`, reduce river visual width and jitter.
4. In `rivers.c`, tighten mouth acceptance so main rivers reach water directly and tributaries either join an accepted river or reach water.
5. In `render.c`, keep the existing cache and avoid a larger caching rewrite unless a later pass explicitly requests it.
