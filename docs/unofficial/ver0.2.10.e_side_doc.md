# Ver0.2.10.e Side Doc

Ver0.2.10.e is a Phase 5 snapshot-cache completion checkpoint over
Ver0.2.10.d. It keeps gameplay behavior unchanged and finishes the immediate
RenderSnapshot cache-slimming work by priming simulation-side presentation
caches before forced snapshot publishes.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Added `src/core/render_snapshot_cache.c` and
   `src/core/render_snapshot_cache.h` as the simulation-side presentation cache
   module for snapshot-ready city, diplomacy, lane, and plague data.
2. Cached city region summaries and city population summaries outside
   `render_snapshot_publish_from_live_state_throttled()`.
3. Cached diplomacy relation, war state, war-front flags, and peace-pressure
   pair data outside the RenderSnapshot publish read-lock path.
4. Cached sea-lane snapshot rows and plague civ/city/lane presentation
   summaries outside the RenderSnapshot publish read-lock path.
5. Added `render_snapshot_cache_update_all()` for finalize paths that mutate
   simulation state and immediately force a snapshot publish.
6. Primed the snapshot presentation cache before forced publishes after world
   generation finalize and load finalize.
7. Primed the snapshot presentation cache before manual-action forced publishes
   such as region regeneration, add/edit civilization, color changes, civil
   unrest, and vassal release.
8. Treated dead city cache entries below `city_count` as valid zero-summary
   entries for the current key so they do not keep city snapshot sections dirty
   forever.
9. Kept the RenderSnapshot publish read-lock path from falling back to expensive
   live summary or diagnostic scans.
10. Added Debug / Performance System visibility for snapshot city cache,
    diplomacy cache, plague cache, and lane snapshot source status.

## No Gameplay Changes

This release does not change gameplay rules, RNG semantics, balance constants,
save format, diplomacy rules, war rules, plague rules, population math,
maritime rules, route-potential rules, sea-lane generation, or world generation
semantics.

## Known Follow-Up

Large-map stutter is not fully solved. Remaining candidates include static
layer rebuild chunking, sea-lane overlay layering, plague animation rendering,
and further scheduler/write-lock slicing.

The synchronous cache-prime helper is intentionally for forced publish finalize
paths. Routine monthly refresh still uses the simulation scheduler's snapshot
cache phase.

## Validation

- `WORLD_SIM_VERSION` is `0.2.10.e`.
- `MAP_SAVE_VERSION` remains 9.
- Build and release checks should include `make -B world_sim.exe`,
  `make check-text`, `git diff --check`, `.c/.h` line-count checks, root
  executable inventory, and bounded executable GUI regression.
