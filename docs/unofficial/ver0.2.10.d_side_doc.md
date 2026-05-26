# Ver0.2.10.d Side Doc

Ver0.2.10.d is a side-panel cache checkpoint over Ver0.2.10.c. It keeps
gameplay behavior unchanged and narrows right-side panel cache invalidation by
splitting the cache by panel tab/subview and separating hover-only repaint from
full panel cache rebuilds.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Split the side-panel cache into cache kinds for collapsed, country list,
   country detail, population, plague, worldgen, debug map, and debug
   performance views.
2. Removed global `render_snapshot_revision()` from the default side-panel data
   key.
3. Narrowed each panel key to the UI and snapshot state that the current panel
   actually displays.
4. Removed unrelated `map_zoom_percent` and `map_legend_collapsed` from
   non-World panel cache keys.
5. Kept map zoom and map legend state in the World panel key because the World
   panel displays those values.
6. Added a hover-only side-panel repaint path that does not increment full
   panel cache invalidation.
7. Added Debug / Performance System visibility for panel key type, refresh
   reason, full invalidation count, hover invalidation count, and throttle
   count.
8. Included `events_revision` in the Country Overview data key so Recent Events
   refresh on event-only snapshot changes.

## No Gameplay Changes

This release does not change map generation, province generation, expansion,
diplomacy, war, plague, population math, ports, maritime rules, sea-lane
generation, route-potential algorithms, save format, or balance constants.

## Known Follow-Up

The next planned phase is snapshot publish slimming. Candidate hot paths include
city summary copying, diplomacy pair/front copying, sea-lane point copying,
plague arrays, and event formatting or other presentation-only derivations that
can be moved outside the read lock or maintained as simulation-side cached
presentation data.

Hover-only repaint still draws the live side panel once for hover feedback. If
hover remains expensive, a future pass can split tooltip/highlight drawing from
the cached panel body.

## Validation

- `WORLD_SIM_VERSION` is `0.2.10.d`.
- `MAP_SAVE_VERSION` remains 9.
- Build and release checks should include `make -B world_sim.exe`,
  `make check-text`, `git diff --check`, `.c/.h` line-count checks, root
  executable inventory, and an executable smoke launch.
