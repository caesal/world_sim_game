# Ver0.2.8 Side Doc

Ver0.2.8 is a stabilization release over Ver0.2.7. It focuses on save/load
continuity, load progress visibility, UI/render responsiveness, plague visual
smoothness, political-map readability, and smoother shallow/deep water
presentation.

This release intentionally does not update or include `docs/official`.

## Main Changes

1. Bumped the visible prototype marker to `0.2.8`.
2. Added `MAP_SAVE_VERSION 8` dynamic-state persistence for diplomacy, war,
   plague, and event-history data.
3. Added load progress state and a central loading overlay so large map loads do
   not look frozen.
4. Preserved loaded diplomacy, vassal, truce, war, plague, and event state
   instead of resetting those systems after loading a complete new save.
5. Split right-panel view-model drawing from map rendering, and kept panel
   invalidation separate from map invalidation.
6. Added layered static-map composition and a non-plague scene cache so plague
   animation can reuse routes, labels, cities, and stable overlays.
7. Reduced plague fog rebuild work with an adaptive lower-resolution fog cache
   and real debug metrics for data, rebuild, and draw timings.
8. Softened political-map colors through render-time saturation limiting and a
   lower political fill alpha.
9. Rebuilt water-depth scoring around a smooth coast-distance and shelf-width
   field, keeping the existing shallow/deep gameplay interfaces.
10. Added water-depth diagnostics for shallow/deep tile counts, shelf-width
    range, and cache rebuild timing.

## Validation

- `make check-text` is required before the Ver0.2.8 commit.
- `git diff --check` is required before the Ver0.2.8 commit.
- `make` is required before the Ver0.2.8 commit.
- Executable smoke launch is required before the Ver0.2.8 commit.
- Full game-flow regression should be reported if completed; otherwise the
  skipped scope and reason must be recorded in the final release report.
