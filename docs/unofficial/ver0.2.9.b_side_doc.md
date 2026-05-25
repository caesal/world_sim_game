# Ver0.2.9.b Side Doc

Ver0.2.9.b is a second render/cache checkpoint over Ver0.2.9. It records the
DecisionSnapshot cache fix and the current large-map validation state before the
next render-side stutter pass.

This release intentionally does not update or include `docs/official`.

## Main Changes

1. Bumped the visible prototype marker to `0.2.9.b`.
2. Added a simulation-side DecisionSnapshot cache.
3. Changed RenderSnapshot civ copying to read cached decisions instead of
   recomputing decision diagnostics under the state read lock.
4. Split civ snapshot copying into `render_snapshot_civs.c`.
5. Added cached-only country and population summary readers for snapshot use.
6. Added budgeted DecisionSnapshot cache refresh during the month scheduler.
7. Refreshed decision cache eagerly after new world generation and map load.
8. Preserved same-identity stale decisions when a fresh cache entry is not ready.
9. Added a safe Waiting fallback for missing decision data.
10. Fixed Waiting and Unknown country decision labels so they do not render as
    Expansion by default.
11. Added decision-cache and snapshot-decision debug rows.
12. Added viewport static, route overlay, and city overlay presentation caches as
    the second render stutter cleanup pass.

## Known Follow-Up

Large-map stutter is still present. The DecisionSnapshot regression is fixed,
but current profiling still points to render-side work:

1. Full viewport dynamic overlays.
2. Label rebuilds.
3. Sea-lane path/dash drawing.
4. Plague animation and fog visuals.

The next pass should keep DecisionSnapshot untouched and focus on render-side
invalidation, overlay cache lifetimes, label throttling, and plague/sea-lane
visual budgets.

## Validation

- `make -B world_sim.exe` is required before the Ver0.2.9.b commit.
- `make check-text` is required before the Ver0.2.9.b commit.
- `git diff --check` is required before the Ver0.2.9.b commit.
- All `.c` and `.h` files must remain at or below 500 lines.
- The repository root should contain exactly one executable: `world_sim.exe`.
- Bounded GUI validation should use a Large map, 26 civilizations, randomized
  physical world, advanced terrain, and civilization settings.
- Decision cache debug rows should show cached decisions with zero fallback in a
  stable large-map run.
- Any remaining stutter must be recorded as known follow-up rather than treated
  as solved.
