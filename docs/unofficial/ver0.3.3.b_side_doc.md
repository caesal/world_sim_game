# Ver0.3.3.b Side Doc

Ver0.3.3.b is a focused capacity and save-version checkpoint over Ver0.3.3.a.
It addresses the remaining mismatch between Extreme-map natural-region
capacity and the global city-slot capacity.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.3.b`.
2. Changed `MAX_CITIES` from the fixed `1024` ceiling to
   `MAX_NATURAL_REGIONS`.
3. Kept `MAX_NATURAL_REGIONS` at `1536`, so Extreme maps now have matching
   natural-region and city-slot capacity.
4. Bumped `MAP_SAVE_VERSION` from `12` to `13` because city and plague-city
   state arrays can now serialize the larger capacity.
5. Updated the in-game pause-menu version summary in English and Chinese.
6. Updated the root README, documentation index, version log, and this side doc.

## Files In Scope

- `src/core/constants.h`
- `src/core/version.h`
- `src/io/map_save.c`
- `src/ui/pause_menu.c`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.3.b_side_doc.md`

## Behavioral Notes

- The change removes the old mismatch where Extreme worlds could generate up
  to `1536` natural regions while city storage still stopped at `1024`.
- Old saves remain loadable through the existing compatibility path.
- New saves use map save version `13` and can record the larger city and
  plague-city state capacity.
- This does not change world generation, region shaping, expansion priorities,
  diplomacy, war, economy, plague formulas, population formulas, routes, or
  balance constants.
- This does not guarantee every natural region will become civilized in a long
  Extreme-map run. Remaining natural pockets can still be caused by
  reachability, port availability, sea-lane contact, or expansion rules.

## Validation

Release validation for this commit should record:

- Canonical `make -B world_sim.exe`.
- `make check-text`, because the pause-menu release text changed.
- `git diff --check`.
- A `.c` include scan confirming no source file includes another `.c` file.
- Touched `.c/.h` line counts under the 500-line limit.
- A touched-file mojibake scan for the edited release text.
- Root executable inventory confirming the repository root only contains
  `world_sim.exe`.

Strict AGENTS Rule39 game-flow regression was not completed for this pushed
checkpoint. This commit must not be treated as full Rule39 release-readiness
evidence until a complete Rule39 run records the final year/month, natural
region count, civilization count, speed setting, five technology-stage-5
civilizations, and deep-sea route hidden-before/revealed-after evidence.

## Residual Risks

- Raising the city-slot ceiling increases memory used by arrays sized from
  `MAX_CITIES`, though the new value matches the already-supported natural
  region maximum.
- Long-run Extreme-map expansion behavior still needs a full Rule39 game-flow
  pass before this version can be called broadly release-ready.
- If natural regions remain unclaimed after the city-cap bottleneck is removed,
  the next diagnosis should inspect reachability, city-port eligibility,
  sea-lane contact, expansion frontier selection, and overseas expansion gates.
