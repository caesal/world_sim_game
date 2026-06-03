# Ver0.3.1.c Side Doc

Ver0.3.1.c is a collapse-partition checkpoint over Ver0.3.1.b. It preserves the
interaction, plague-cooldown, fragmentation-control, marker, route, and
war-cadence work while making multi-province Civil Unrest collapses produce
larger, more balanced, connected successor territories.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.1.c`.
2. Added `src/sim/collapse_partition.c/.h`.
3. Updated `Makefile` and `build.bat` for the new collapse partition module.
4. Replaced the old multi-province collapse scan that picked successor seeds by
   natural region id order and capped each successor at 6 regions.
5. Added successor count bands:
   - 2-35 owned regions: 1 successor
   - 36-72 owned regions: 2 successors
   - 73-128 owned regions: 3 successors
   - 129-172 owned regions: 4 successors
   - more than 172 owned regions: 5 successors
6. Added capital-core retention, farthest-first successor seeding, balanced
   multi-source BFS assignment, and internal successor-capital selection.
7. Preserved single-province collapse behavior.

## Behavioral Notes

- The original country keeps the capital-seeded block.
- Successor blocks are built from connected natural-region graph expansion and
  aim for balanced sizes rather than tiny fixed-size fragments.
- Successor capitals are selected inside their assigned block instead of using
  the farthest seed blindly.
- Slot shortage safely reduces successor count.
- Civil Unrest pause/run preservation, manual grace exception, battle cadence,
  vassal behavior, enclave behavior, plague, routes, world generation, and
  `MAX_CIVS` were not intentionally changed.

## Validation

- `WORLD_SIM_VERSION` is `0.3.1.c`.
- Build/static validation was reported passing by the implementation agent and
  rerun for the release.
- Focused probes reported:
  - single-province behavior unchanged
  - 20-region country creates 1 successor
  - 50/100/150/180+ region partition cases create the expected successor counts
    and connected balanced blocks
  - a 100-region actual collapse produced parent/children sized around
    26/25/25/24
  - slot shortage reduces requested successors safely
- The implementation agent's GUI automation was stopped by the user before
  automated focused GUI and strict AGENTS validation completed.
- The user manually validated the executable result and approved this checkpoint
  for push.

## Residual Risks

- This checkpoint relies on focused probes plus user manual GUI validation; full
  automated 26-civilization strict AGENTS regression was not rerun.
- Future collapse tuning should report successor count, parent size, child
  sizes, connectivity, capital placement, and slot-shortage handling together.
