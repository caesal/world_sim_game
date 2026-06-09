# Ver0.3.2.e Side Doc

Ver0.3.2.e is a broad simulation, economy, UI, and render-performance checkpoint
over Ver0.3.2.d. It accepts the treasury/resource-pressure WIP, population
diagnostics, war-economy hooks, selected-detail performance fixes, and the
political-map ownership latency fix.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.2.e`.
2. Added a persistent capped `treasury` layer that remains separate from
   commerce/trade capability and local money potential.
3. Added annual economy settlement, treasury caps, treasury surplus/deficit
   buffering, resource-pressure calculation, stability spending, war indemnity
   offsets, and temporary mercenary support hooks.
4. Added save compatibility plumbing for the expanded civilization state.
5. Updated population diagnostics with effective population pressure,
   weighted fertility, proportional pressure deaths, small-population natural
   mortality, Top 6 city rows, city display names, type/status columns, and a
   denser visual Population tab.
6. Updated decision/war desire diagnostics so resource crisis with no reachable
   expansion target can raise war pressure while existing hard gates remain.
7. Added treasury/resource-pressure display in Country overview/resources and
   Debug / Performance diagnostics.
8. Added runtime-only profiling switches for render investigation, including
   side-panel detail draw, highlight, city overlay, labels, static scene,
   diplomacy animation, map legend, and panel cache rebuild paths.
9. Batched selected-country highlight work, added contour/edge caching, and
   reduced highlight hot-path GDI churn.
10. Split static map and static scene cache responsibilities so stale safe
    frames no longer hide current political ownership colors.
11. Added political fill/border publish diagnostics and fixed ownership-color
    latency so current political fill and borders can publish before full
    coast/hydro/static compose completion.

## Files In Scope

- `Makefile`
- `build.bat`
- `src/core/version.h`
- `src/core/dirty_flags.*`
- `src/core/event_log*`
- `src/core/render_snapshot*`
- `src/game/game_loop.*`
- `src/game/game_crisis_probe.c`
- `src/game/game_economy_probe.c`
- `src/io/map_save*`
- `src/render/map_highlight*`
- `src/render/map_label_cache*`
- `src/render/panel_country*`
- `src/render/panel_debug*`
- `src/render/profiling_switches*`
- `src/render/render*`
- `src/render/sea_lane_render*`
- `src/render/snapshot_ui*`
- `src/sim/collapse.c`
- `src/sim/decision_snapshot*`
- `src/sim/disorder.c`
- `src/sim/economy*`
- `src/sim/enclave_resolution.c`
- `src/sim/expansion.c`
- `src/sim/population*`
- `src/sim/simulation*`
- `src/sim/stability_decision.c`
- `src/sim/technology.c`
- `src/sim/war*`
- `src/ui/ui*`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.2.e_side_doc.md`

## Behavioral Notes

- Commerce remains the civilization trade/economic capability metric, and local
  money remains the current territorial money potential. Treasury is the new
  persistent, spendable national reserve.
- Resource pressure stays in the 0-100 pressure family and is now tied to
  treasury buffering and population capacity pressure.
- Population diagnostics are presentation/diagnostic-facing, but the accepted
  WIP also adjusts fertility weighting, pressure death distribution, and small
  natural mortality handling.
- Static map presentation now distinguishes stale-presentable frames from
  current boundary-safe frames. Political/All/Regions modes may publish current
  political fill and border layers before full static composition is complete.
- Runtime profiling switches are diagnostic-only and are not intended as final
  gameplay or product behavior.

## Validation

- `WORLD_SIM_VERSION` is `0.3.2.e`.
- Focused pre/post ownership latency evidence reproduced stale political
  publish lag at 143250 ms before the fix and reduced focused post-fix max
  pending latency to 78 ms.
- Rule39 ownership validation reported max political pending latency of 31 ms.
- Selected-detail matrix validation stayed around 9.17-11.36 months/sec without
  repeated 235-250 ms selected-detail peaks.
- Full Rule39 validation used a Large map with 26 initial civilizations, 712
  natural regions, max/5x speed, reached Year 435 Month 6, and had 24 stage-5
  civilizations plus 6 stage-6 civilizations.
- Deep-sea evidence reported visible deep routes transitioning from 0 before
  unlock to 2 after unlock.
- Canonical `make -B world_sim.exe` was attempted first and reached the link
  step, but the running `world_sim.exe` was locked by PID 48996.
- A temporary-target build with `TARGET=tmp_worldsim_ver032e_verify.exe`
  succeeded, string checks found `World Sim Game Ver 0.3.2.e`, and the
  temporary executable was deleted.
- `cmd /c build.bat` was attempted and reached the link step, but was blocked
  by the same locked canonical executable.
- `make check-text` passed.
- `git diff --check` passed with only CRLF conversion warnings.
- Static checks found no `.c` file includes another `.c`.
- All touched source/header files are at or below 500 lines.
- Root executable inventory contains exactly `world_sim.exe`.

## Residual Risks

- Rule39 recorded isolated late-run frame peaks up to 250 ms during late
  technology/deep-sea transition. Throughput remained near 10 months/sec, so
  this is recorded as a watch item rather than a blocker for this checkpoint.
- The worktree accepted by this release is broad and includes gameplay,
  simulation, save, UI, and render changes; future changes should be split into
  narrower reviewable checkpoints again.
