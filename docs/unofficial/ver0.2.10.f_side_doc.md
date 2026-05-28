# Ver0.2.10.f Side Doc

Ver0.2.10.f is an AGENTS validation-policy restoration checkpoint over
Ver0.2.10.e. The rejected Phase 6 performance experiments were rolled back;
this checkpoint preserves the Ver0.2.10.e gameplay/code baseline while
restoring the stricter validation rules future Phase 6 and performance work
must satisfy.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.2.10.f`.
2. Restored the strict Phase 6/performance validation rule in `AGENTS.md`.
3. Required Large-map validation with at least 26 placed civilizations,
   randomized physical map parameters, randomized advanced terrain
   preferences, more than 600 natural regions, and 5x/max speed until at least
   five distinct civilizations reach technology stage 10.
4. Required final validation reports to include final year/month, natural
   region count, confirmed civilization count, speed setting, and exact ids and
   names for five technology-stage-10 civilizations.
5. Restored the executable-validation safety rule that agents must not force
   `world_sim.exe` to the foreground while another fullscreen application is
   active.
6. Restored the maximized-window validation rule so the Debug / Performance
   panel is fully visible, resized, scrolled, captured, or transcribed before
   performance evidence is accepted.
7. Updated the root README, documentation index, and unofficial version log for
   Ver0.2.10.f.

## No Gameplay Changes

This release does not change gameplay rules, RNG semantics, balance constants,
save format, diplomacy rules, war rules, plague rules, population math,
maritime rules, route-potential rules, sea-lane generation, world generation
semantics, rendering code, or simulation code.

## Known Follow-Up

Phase 6 large-map performance remains unsolved and should be treated as
rejected until a future implementation passes the strict validation gate. New
work should restart from the Ver0.2.10.f baseline, diagnose with code and
runtime evidence first, and avoid claiming acceptance from cropped panels,
bounded 60-second runs, or incomplete technology-stage evidence.

## Validation

- `WORLD_SIM_VERSION` is `0.2.10.f`.
- `MAP_SAVE_VERSION` remains 9.
- Build and release checks should include `make -B world_sim.exe`,
  `make check-text`, `git diff --check`, `.c/.h` line-count checks, and root
  executable inventory.
- Full GUI gameplay regression is not required for this release because it only
  changes repository instructions, version metadata, and release documentation.
