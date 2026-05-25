# Ver0.2.10 Side Doc

Ver0.2.10 is a presentation-ordering and Windows polish checkpoint over
Ver0.2.9.c. It records the snapshot gate for diplomacy map animations, safer
static map cache revision semantics, and executable icon packaging.

This release intentionally does not update or include `docs/official`.

## Main Changes

1. Bumped the visible prototype marker to `0.2.10`.
2. Added a Windows resource script and app icon assets to the canonical build.
3. Updated the Makefile to compile the resource object into `world_sim.exe`.
4. Added snapshot focus endpoint fields to `SnapshotCiv`.
5. Changed diplomacy map animations to consume only events present in the
   currently rendered `RenderSnapshot`.
6. Replaced live animation endpoints with snapshot-consistent civ endpoints.
7. Validated diplomacy contact and war-front animation eligibility against
   snapshot diplomacy/front state.
8. Delayed new diplomacy map animations when the static map cache has not yet
   presented the same snapshot.
9. Changed static map cache keys to use snapshot revision fields when drawing
   snapshot pixels.
10. Guarded render dirty clearing so stale snapshot pixels do not acknowledge a
    newer live dirty revision.
11. Added Debug / Performance System rows for diplomacy animation source,
    delayed state, consumed event totals, and snapshot/static/live map
    revisions.

## No Gameplay Changes

This release does not change world generation, region generation, expansion,
population, plague, diplomacy, war, vassal, maritime, sea-lane, route-potential,
or balance rules. The changes are restricted to executable packaging,
presentation ordering, render cache semantics, and debug visibility.

## Known Follow-Up

Large-map stutter is still present. The diplomacy animation race is gated by
snapshot/static presentation state, but profiling still shows late-game pressure
from plague animation, static-dirty invalidation, maritime drawing, label work,
and general simulation backlog on large maps.

## Validation

- `make world_sim.exe` is required before the Ver0.2.10 commit.
- `make check-text` is required before the Ver0.2.10 commit.
- `git diff --check` is required before the Ver0.2.10 commit.
- All `.c` and `.h` files must remain at or below 500 lines.
- The repository root should contain exactly one executable: `world_sim.exe`.
- GUI validation should use a Large map, 26 civilizations, randomized physical
  world, advanced terrain, and civilization settings.
- Debug validation should confirm diplomacy animations report `snapshot` source
  and delay when the static map has not caught up.
- Pan/zoom validation should confirm cities, capitals, ports, labels, routes,
  and borders remain attached with no blank blue map.
