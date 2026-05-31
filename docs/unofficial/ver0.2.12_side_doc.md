# Ver0.2.12 Side Doc

Ver0.2.12 is a port-display, port-density, and release-hygiene checkpoint over
Ver0.2.11. It preserves the one natural-region / one generated city-slot model
while making port cities behave as the single visible city marker for their
province instead of drawing or targeting separate city and harbor points.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.2.12`.
2. Added `src/core/city_display.c` and `src/core/city_display.h` for the shared
   visible point used by normal city slots and port-city harbor markers.
3. Updated city rendering so a port city draws only the harbor marker at the
   coast, not both a city icon and a port icon.
4. Drew port capitals with the same extra capital ring treatment used by normal
   capitals.
5. Updated country focus, snapshot focus points, highlight rings, label
   anchors, and map hit-testing to use the shared display point.
6. Updated the selected-country overview so `Ports` replaces the duplicate
   `Cities` count; `Provinces` remains the total one-city-per-region count.
7. Reworked port policy so candidate discovery is private to the policy pass
   and `NaturalRegion.has_port_site` means final actual port state.
8. Reduced non-forced coastal port density while keeping the island
   land-component guarantee for at least one valid port.
9. Synchronized `Makefile` and `build.bat` after the new helper module split.
10. Recorded the UI/UX Claymorphism presentation rules in `AGENTS.md`.
11. Cleaned touched and over-limit `.c` files back under the 500-line project
    rule without changing gameplay semantics.

## Behavioral Notes

- A natural region still has exactly one generated local city slot.
- A port city is that region's one city with port fields set; it is not an
  additional city entity.
- The harbor marker is the display point for a port city. Labels, selection,
  country focus, highlight rings, and hit-testing should agree with that point.
- `NaturalRegion.has_port_site` should be read as final actual port state after
  the policy pass, not as a transient coastal-candidate flag.
- The country panel shows province count and port count separately. Ordinary
  non-port city count is `provinces - ports`.

## Known Follow-Up

Phase 6 large-map performance remains outside this release scope. Any future
performance, stutter, scheduler, rendering, map-display, or simulation-speed
work must pass the strict AGENTS regression before acceptance.

If a release task does not complete the current maximized GUI/game-flow strict
regression, this checkpoint should be treated as pushed but not broadly
gameplay-accepted.

## Validation

- `WORLD_SIM_VERSION` is `0.2.12`.
- `MAP_SAVE_VERSION` remains 10.
- Required release checks are `make -B world_sim.exe`, `make check-text`,
  `git diff --check`, `.c/.h` line-count checks, root executable inventory,
  static keyword scans, focused GUI validation, and the current AGENTS strict
  regression.
