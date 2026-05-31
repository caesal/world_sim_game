# Ver0.2.12.c Side Doc

Ver0.2.12.c is a max-speed interaction responsiveness checkpoint over
Ver0.2.12.b. It keeps gameplay, simulation rules, world generation, diplomacy,
war, plague, population, economy, technology, province semantics, and balance
behavior unchanged while reducing UI stalls during 5x/max-speed interaction.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.2.12.c`.
2. Removed the side-panel handle click path's synchronous full-window
   `UpdateWindow` call.
3. Added `render_paint_side_panel_now()` as a narrow side-panel immediate paint
   path for tab and subtab navigation.
4. Routed `ui_invalidate_side_panel_immediate()` through the narrow side-panel
   paint path instead of a full synchronous window update.
5. Added cached-window blitting when full-map paint work is requested while
   input is waiting, so input events do not sit behind heavy map redraws.
6. Coalesced presentation redraws at max speed when render or simulation
   pressure is high.

## Behavioral Notes

- This release is a rendering/UI responsiveness checkpoint, not a gameplay
  rule checkpoint.
- Month progression, technology progression, war behavior, population,
  worldgen, route rules, province ownership, and balance constants are intended
  to remain unchanged.
- Side-panel tab and subtab changes should repaint immediately through the
  side-panel path.
- Heavy map repaint work may be deferred briefly while input is queued, using
  the cached backbuffer until the full repaint can run.

## Known Follow-Up

- The direct side-panel immediate paint path should remain centralized. Future
  immediate UI paths should use a deliberate partial-paint abstraction instead
  of adding more ad-hoc `GetDC` drawing sites.
- Broader Phase 6 performance work may continue if later profiling finds new
  bottlenecks.
- UI/UX Claymorphism Phase 2B should continue separately from this performance
  checkpoint.

## Validation

- `WORLD_SIM_VERSION` is `0.2.12.c`.
- `MAP_SAVE_VERSION` remains 10.
- Build/static validation passed for the performance responsiveness scope.
- Strict AGENTS regression was reported complete with:
  - Large map.
  - 26 initial civilizations.
  - 654 natural regions.
  - Randomized physical parameters.
  - Randomized advanced terrain preferences.
  - Max speed, target 0.1s/month.
  - Final observed time: Year 424 Month 11.
  - Five distinct civilizations at technology stage 5 or beyond:
    civ id 8 / Country 9, civ id 73 / Country 74, civ id 78 / Country 79,
    civ id 158 / Country 159, and civ id 146 / Country 147.
  - Deep-sea route evidence: lanes hidden before unlock and active lane data
    after unlock, including shallow 96 / deep 33 route stats.
