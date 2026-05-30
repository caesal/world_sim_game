# Ver0.2.11 Side Doc

Ver0.2.11 is a province, settlement, war-cession, and render-cache checkpoint
over Ver0.2.10.f. It records the move toward natural regions as the single
province unit, with exactly one generated city slot per region and port cities
represented as a subtype of that same city.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.2.11`.
2. Raised `MAP_SAVE_VERSION` to 10 and normalized settlement/port region state
   after load.
3. Added `src/sim/regions_balance.c` and `src/sim/regions_balance.h` for
   shared natural-region size bands and merge target scoring.
4. Rebalanced province size cleanup so tiny regions merge by target fit and
   huge regions split against explicit hard/soft thresholds.
5. Added `src/sim/regions_settlement.c` and
   `src/sim/regions_settlement.h` to keep one stable local city slot per
   natural region.
6. Added `src/sim/regions_port_policy.c` and
   `src/sim/regions_port_policy.h` for full-pass port-city assignment.
7. Kept ports as a city subtype: a region has either a normal city or a port
   city, not both.
8. Guaranteed island land components at least one port city when a valid
   coastal candidate exists, while allowing additional coastal ports through
   the normal policy.
9. Changed war cession accounting and transfer selection to use owned natural
   regions instead of legacy city-index province ids.
10. Preserved generated city position, port flag, port position, and port
    region across claim and war transfer.
11. Rendered neutral generated settlement slots only on the Regions map layer;
    other normal layers hide unowned city and port icons.
12. Exposed neutral settlement and city overlay cache counters in the Debug /
    Performance panel.
13. Kept the side-panel collapse, handle dirty-rect, route legend, and
    lightweight repaint fixes in scope.
14. Updated AGENTS strict validation expectations to stage 5 plus explicit
    deep-sea route hidden-before/revealed-after evidence.

## Behavioral Notes

- A generated natural region should always have exactly one local city slot.
- A port city is the region's one city with port fields set, not an additional
  entity.
- Unowned settlements exist in generated data, but normal gameplay layers hide
  their city and harbor markers until the region is occupied.
- The Regions map layer may reveal neutral generated settlement slots with
  neutral styling.
- War cession transfers natural regions and activates the transferred region's
  local city for the winner without rerolling port policy.

## Known Follow-Up

Phase 6 large-map performance remains outside this release scope. Any future
performance, stutter, scheduler, rendering, map-display, or simulation-speed
work must pass the strict AGENTS regression before acceptance.

## Validation

- `WORLD_SIM_VERSION` is `0.2.11`.
- `MAP_SAVE_VERSION` is 10.
- Required release checks are `make -B world_sim.exe`, `make check-text`,
  `git diff --check`, `.c/.h` line-count checks, root executable inventory,
  static keyword scans, focused GUI validation, and the current AGENTS strict
  regression.
