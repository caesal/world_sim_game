# Ver0.2.10.c Side Doc

Ver0.2.10.c is a label-cache checkpoint over Ver0.2.10.b. It keeps gameplay
behavior unchanged and narrows map-label work during pan/zoom by splitting label
source collection from screen placement.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Added `src/render/map_label_cache.c` and `.h`.
2. Moved map label cache implementation out of `src/render/map_labels.c`.
3. Preserved `draw_map_labels()` as the public draw-entry wrapper.
4. Split label source/candidate data from screen placement data.
5. Removed camera, viewport, raw pan, mouse, and frame state from the label
   source key.
6. Kept placement keyed to viewport/layout buckets, display mode, semantic zoom
   LOD, tile size, and selection.
7. Skipped full label placement/collision work during interaction preview.
8. Added debug rows for label source rebuilds, placement rebuilds, preview
   skips, drawn labels, and source/placement reasons.
9. Added the new source file to the canonical Makefile build.

## No Gameplay Changes

This release does not change map generation, province generation, expansion,
diplomacy, war, plague, population math, ports, maritime rules, sea-lane
generation, route-potential algorithms, save format, or balance constants.

## Known Follow-Up

The next planned phase is side-panel cache splitting. The current global panel
view-model cache can still be invalidated by unrelated state such as map zoom,
legend state, hover movement, broad snapshot revisions, or debug refreshes.

`src/render/map_label_cache.c` is exactly 500 lines. Future label work should
split it before adding logic.

## Validation

- `WORLD_SIM_VERSION` is `0.2.10.c`.
- `MAP_SAVE_VERSION` remains 9.
- Build and release checks should include `make -B world_sim.exe`,
  `make check-text`, `git diff --check`, `.c/.h` line-count checks, root
  executable inventory, and an executable smoke launch.
