# Ver0.2.12.d Side Doc

Ver0.2.12.d is a recoverable performance, UI/UX, province-shape, route-display,
and render-cache checkpoint over Ver0.2.12.c. It exists to archive the current
successful responsiveness state before the next city/harbor marker visual pass.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.2.12.d`.
2. Preserved the current Claymorphism Phase 2B shell/control polish and its
   blocker fixes.
3. Preserved the current route-potential legend, sea-lane dash overlap, and
   selected-highlight display follow-ups.
4. Preserved the current province/region-shape follow-ups and validation state.
5. Preserved the render-cache split through `src/render/render_layer_cache.c`
   and `src/render/render_layer_cache.h`, with both build lists updated.
6. Preserved the successful max-speed stutter, year-jump, stale-snapshot, and
   UI responsiveness fixes.

## Behavioral Notes

- This release is an archival checkpoint before the marker-icon follow-up.
- The user confirmed that the latest performance pass substantially improved
  gameplay stutter, year jumps, UI click responsiveness, and general large-map
  interaction.
- Gameplay rule changes are not the purpose of this checkpoint.
- The current map city and harbor markers use simplified lightweight glyphs.
  This is a known visual regression to fix next, ideally through cached
  original or near-original marker sprites rather than per-marker GDI+ drawing.

## Known Follow-Up

- Restore recognizable city and harbor marker visuals without reintroducing
  the earlier marker-rendering stutter.
- Verify whether the old per-marker PNG/GDI+ draw path was a primary bottleneck.
- If it was, use cached marker sprites or recognizable lightweight vector icons.
- If it was not, restore the original marker icon visual path with evidence.

## Validation

- `WORLD_SIM_VERSION` is `0.2.12.d`.
- `MAP_SAVE_VERSION` remains 10.
- The user confirmed major real-play improvement after the latest performance
  pass.
- The performance pass reported strict AGENTS regression with:
  - Large map.
  - 26 initial civilizations.
  - 751 natural regions.
  - City count 751.
  - Randomized physical and advanced terrain parameters.
  - Max speed / index 4, target 0.1s/month.
  - Final observed time: Year 579 Month 7.
  - Five distinct civilizations at technology stage 5 or beyond:
    civ 0 / Westmarch Realm, civ 1 / Eastern YinShan Court,
    civ 2 / Kurotsuki Shogunate, civ 3 / The Black Banner,
    and civ 4 / Ironmere Confederacy.
  - Deep-sea route evidence before and after unlock, including deep links
    increasing after technology unlock.
