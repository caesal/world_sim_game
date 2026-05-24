# Ver0.2.9.a Side Doc

Ver0.2.9.a is a render/cache checkpoint over Ver0.2.9. It records the first
cache-invalidation pass before the next large-map performance pass.

This release intentionally does not update or include `docs/official`.

## Main Changes

1. Bumped the visible prototype marker to `0.2.9.a`.
2. Retired the old screen-space `map_scene_cache` because it mixed static map
   content with dynamic city, port, and maritime overlays.
3. Kept maritime routes, plague visuals, city/capital/port icons, labels,
   country highlights, diplomacy animations, and selected-tile markers as live
   overlays over the durable static map cache.
4. Removed the default completed-month dynamic map redraw request.
5. Split label invalidation into narrower country and city/port revision paths.
6. Narrowed population-only and civ-stat dirty paths so they no longer rebuild
   labels or city-icon layout by default.
7. Added debug rows for completed-month map redraw, map invalidation reason,
   retired scene cache status, label revisions, map icon counts, and route
   geometry reuse.

## Known Follow-Up

Large-map stutter is still present. The current profiling evidence points to:

1. RenderSnapshot civ-section publishing doing expensive work while holding the
   simulation state read lock.
2. Full viewport rendering still redrawing dynamic overlays too often after the
   old scene cache was retired.

The next pass should focus on snapshot read-model caching and safe viewport
presentation caches without restoring the old mixed scene cache behavior.

## Validation

- `make check-text` is required before the Ver0.2.9.a commit.
- `git diff --check` is required before the Ver0.2.9.a commit.
- `make all` is required before the Ver0.2.9.a commit.
- Bounded GUI validation should use a Large map, 26 civilizations, randomized
  physical world, advanced terrain, and civilization settings.
- Any remaining stutter must be recorded as known follow-up rather than treated
  as solved.
