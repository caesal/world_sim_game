# Ver0.2.9.c Side Doc

Ver0.2.9.c is a plague performance checkpoint over Ver0.2.9.b. It records the
debug-only plague switches used to isolate whether stutter is coming from plague
map visuals, plague simulation effects, or unrelated late-game work.

This release intentionally does not update or include `docs/official`.

## Main Changes

1. Bumped the visible prototype marker to `0.2.9.c`.
2. Added a centralized plague performance switch module.
3. Added Debug / Performance System controls for `Plague System` and
   `Plague Map Visuals`.
4. Kept both switches default ON so normal gameplay behavior is unchanged.
5. Allowed plague map visuals to be disabled independently from plague
   simulation.
6. Suppressed plague-animation map invalidation when plague visuals are OFF.
7. Skipped plague fog, pulse, and infected sea-lane visuals when plague visuals
   are OFF.
8. Allowed the plague system to be disabled for profiling.
9. Skipped random outbreaks, monthly plague updates, migration exposure, war
   exposure, disorder plague contribution, and plague population deaths when the
   plague system is OFF.
10. Added debug rows for switch state, skipped simulation, skipped visuals,
    suppressed invalidation, and plague visual reason state.

## Known Follow-Up

Large-map stutter is still present. This release adds isolation switches rather
than claiming a full performance fix. Current profiling points to several
remaining candidates:

1. Full viewport dynamic overlays.
2. Label rebuild and draw pressure.
3. Sea-lane path and dash rendering.
4. Calendar/scheduler stalls.
5. Diplomacy and war churn.
6. Late-game invalidation coalescing and presentation cache lifetimes.

When the plague system is OFF, existing plague snapshot state is preserved rather
than cleared. The switch disables ongoing plague simulation/effects and map
visual invalidation; it does not rewrite historical plague data already present
in panels.

## Validation

- `make -B world_sim.exe` is required before the Ver0.2.9.c commit.
- `make check-text` is required before the Ver0.2.9.c commit.
- `git diff --check` is required before the Ver0.2.9.c commit.
- All `.c` and `.h` files must remain at or below 500 lines.
- The repository root should contain exactly one executable: `world_sim.exe`.
- Bounded GUI validation should use a Large map, 26 civilizations, randomized
  physical world, advanced terrain, and civilization settings.
- Debug validation should confirm both plague switches default ON, plague visual
  invalidation stops when map visuals are OFF, and plague simulation/effect work
  is skipped when the system switch is OFF.
- Any remaining stutter must be recorded as known follow-up rather than treated
  as solved.
