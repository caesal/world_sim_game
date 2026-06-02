# Ver0.3.1.a Side Doc

Ver0.3.1.a is a plague-cooldown and high-load presentation checkpoint over
Ver0.3.1. It preserves the fragmentation-control work while adding a persistent
city-level plague recovery cooldown and keeping the large-map max-speed UI
responsiveness improvements from the validation pass.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.1.a`.
2. Added `PlagueState.reinfection_cooldown_months`.
3. Set recovered cities to a hard 432-month reinfection cooldown.
4. Routed direct, random, local, maritime, migration, and war-casualty plague
   infection attempts through the city cooldown gate.
5. Bumped `MAP_SAVE_VERSION` to 11 because plague city save state now includes
   the cooldown field.
6. Added v10 `PLGC` legacy conversion that initializes the new cooldown field
   to 0 for old saves.
7. Increased high-load presentation coalescing intervals at max speed so
   overloaded Large-map rendering pressure does not block UI interaction as
   aggressively.

## Behavioral Notes

- City plague immunity still exists as a chance/severity modifier, but the new
  city cooldown is a hard no-reinfection window after recovery.
- Country-level random plague immunity remains separate from city-level
  reinfection cooldown.
- Active plague can still be extended while a city is currently infected; the
  hard gate applies after recovery.
- The presentation coalescing change is visual-only and does not alter
  simulation month progression, plague math, diplomacy, war, expansion,
  world generation, RNG, or balance constants.

## Validation

- `WORLD_SIM_VERSION` is `0.3.1.a`.
- `MAP_SAVE_VERSION` is 11.
- Build/static validation was reported passing for the implementation.
- Focused probes reported:
  - recovered city cooldown is 432 months
  - direct seed is blocked during cooldown
  - local, maritime, migration, and war reinfection paths are blocked
  - cooldown decrements and infection works again at 0
  - new saves preserve cooldown
  - legacy v10 saves load cooldown as 0
- The user manually validated the executable experience and approved this
  checkpoint for push.

## Residual Risks

- Future Large-map performance work should keep checking for stale map colors,
  route visibility, city/port markers, plague overlays, and delayed highlight
  updates when presentation coalescing is active.
- Future plague changes should report random immunity, city cooldown, active
  plague count, deaths, and plague disorder pressure separately.
