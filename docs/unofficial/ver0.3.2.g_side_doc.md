# Ver0.3.2.g Side Doc

Ver0.3.2.g is a focused population age-structure checkpoint over Ver0.3.2.f.
It adds display-only yearly cohorts for population pyramid rendering and updates
old-age natural mortality while preserving the existing real gameplay cohort
storage.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.2.g`.
2. Added `src/sim/population_display_cohorts.c/.h` as a display-only yearly
   cache for age 0 through 100+.
3. Added `src/sim/population_mortality.c/.h` for old-age natural mortality
   formulas and the internal split of the real `75+` bucket.
4. Kept real population storage on the existing 8 `PopulationCohort` gameplay
   bands; there is no save-format change and no new real `81+` gameplay cohort.
5. Kept the visible UI row as `75+`; the UI does not expose a separate `81+`
   row.
6. Updated population pyramid bar scaling to use density-style values:
   5-year rows divide by 5, and the visible `75+` row divides by 12.
7. Updated monthly old-age natural mortality:
   - 55-64 = n / 200
   - 65-74 = n / 83
   - 75-80 = n / 24
   - 81+ = n * 8 / 100
8. Added a focused `--probe-population` validation path for formula, split,
   display-wave, display-scale, and 3-seed Year-240 balance evidence.

## Files In Scope

- `src/core/version.h`
- `Makefile`
- `build.bat`
- `src/main.c`
- `src/game/game.h`
- `src/game/game_population_probe.c`
- `src/core/render_snapshot.h`
- `src/core/render_snapshot_civs.c`
- `src/sim/population.c`
- `src/sim/population_diagnostics.c/.h`
- `src/sim/population_display_cohorts.c/.h`
- `src/sim/population_mortality.c/.h`
- `src/render/panel_country_population.c`
- `src/render/panel_population.c`
- `src/render/panel_population_page.c`
- `src/render/render_panel_internal.h`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.2.g_side_doc.md`

## Behavioral Notes

- The display cohort cache is presentation support. It lets visible pyramid
  waves move upward over time without changing real storage to yearly cohorts.
- Old-age mortality uses the display cache only to estimate how much of the real
  `75+` bucket belongs to 75-80 versus 81+.
- Actual gameplay deductions still come from real 55-64, real 65-74, and real
  75+ buckets.
- Birth multiplier, pressure-death curve, fertility weights, birth divisor,
  treasury, resource pressure, war, plague, routes, diplomacy, world generation,
  and map-cache behavior are intentionally unchanged from Ver0.3.2.f.

## Validation

- `WORLD_SIM_VERSION` is `0.3.2.g`.
- Focused formula probes reported monthly natural deaths of 5000, 12048, 41667,
  and 80000 for 1,000,000 people in 55-64, 65-74, 75-80, and 81+.
- Split fixture reported 60K in 75-80 plus 40K in 81+ removed 5700 from the
  real `75+` bucket, while the visible row remained `75+`.
- Display-wave probes reported a birth batch reaching `5-9` after 6 years and
  `20-24` after 20 years.
- Real gameplay aging remained on the existing 8-band movement model.
- Three Large-map, 26-civilization, >600-region Year-240 probes ended with:
  - seed 161670: 728 regions, 29/29 civs alive, population/capacity 98%
  - seed 272892: 735 regions, 28/27 civs alive, population/capacity 100%
  - seed 391316: 715 regions, 28/27 civs alive, population/capacity 90%
- The focused Population-tab max-speed sample reported 93 ms/month,
  10.75 months/sec, queue 1, coalesced 0, frame 76/188, and render 63/125.
- GUI evidence covered country Population English/Chinese, elderly fixtures,
  world Population English/Chinese, and Debug / Performance.
- Strict AGENTS Rule39 was not rerun for this checkpoint; this release is based
  on focused probes plus user acceptance after manual inspection.
- Canonical `make -B world_sim.exe` was attempted first and reached the link
  step, but the running `world_sim.exe` was locked by PID 29828.
- A temporary-target build with `TARGET=tmp_worldsim_ver032g_verify.exe`
  succeeded, string checks found `World Sim Game Ver 0.3.2.g`, and the
  temporary executable was deleted.
- `cmd /c build.bat` was attempted and reached the link step, but was blocked
  by the same locked canonical executable.
- `make check-text` and `git diff --check` passed.
- Static checks found no `.c` file includes another `.c`.
- All touched source/header files are at or below 500 lines.

## Residual Risks

- Old-age mortality is materially stronger than Ver0.3.2.f and remains a
  longer-run balance watch item.
- The 3-seed Year-240 probes ended with negative average monthly net growth and
  two near-extinct civilization cases.
- `src/sim/population.c` is exactly 500 lines; future edits there must split by
  responsibility before adding code.
